from __future__ import annotations

import copy
import numbers
import pathlib
import sys
import re
import mmap
import struct
import array
import io
from collections import abc
import functools
import types
from typing import Tuple, Union, List, Iterable, Any, Optional, Pattern, Dict, \
    BinaryIO, TextIO, Callable, overload, Iterator, Type, TypeVar
import bitarray
import bitarray.util
from bitstring.utils import tokenparser, BYTESWAP_STRUCT_PACK_RE, STRUCT_SPLIT_RE
from bitstring.exceptions import CreationError, InterpretError, ReadError, Error
from bitstring.fp8 import fp143_fmt, fp152_fmt
from bitstring.bitstore import BitStore, _offset_slice_indices_lsb0

# Things that can be converted to Bits when a Bits type is needed
BitsType = Union['Bits', str, Iterable[Any], bool, BinaryIO, bytearray, bytes, memoryview, bitarray.bitarray]

TBits = TypeVar("TBits", bound='Bits')

byteorder: str = sys.byteorder

# An opaque way of adding module level properties. Taken from https://peps.python.org/pep-0549/
_bytealigned: bool = False
_lsb0: bool = False

# The size of various caches used to improve performance
CACHE_SIZE = 256


class _MyModuleType(types.ModuleType):
    @property
    def bytealigned(self) -> bool:
        """Determines whether a number of methods default to working only on byte boundaries."""
        return globals()['_bytealigned']

    @bytealigned.setter
    def bytealigned(self, value: bool) -> None:
        """Determines whether a number of methods default to working only on byte boundaries."""
        globals()['_bytealigned'] = value

    @property
    def lsb0(self) -> bool:
        """If True, the least significant bit (the final bit) is indexed as bit zero."""
        return globals()['_lsb0']

    @lsb0.setter
    def lsb0(self, value: bool) -> None:
        """If True, the least significant bit (the final bit) is indexed as bit zero."""
        value = bool(value)
        _switch_lsb0_methods(value)
        globals()['_lsb0'] = value


sys.modules[__name__].__class__ = _MyModuleType

# Maximum number of digits to use in __str__ and __repr__.
MAX_CHARS: int = 250


def tidy_input_string(s: str) -> str:
    """Return string made lowercase and with all whitespace and underscores removed."""
    try:
        l = s.split()
    except (AttributeError, TypeError):
        raise ValueError(f"Expected str object but received a {type(s)} with value {s}.")
    return ''.join(l).lower().replace('_', '')


# Size in bytes of all the pack codes.
PACK_CODE_SIZE: Dict[str, int] = {'b': 1, 'B': 1, 'h': 2, 'H': 2, 'l': 4, 'L': 4,
                                  'q': 8, 'Q': 8, 'e': 2, 'f': 4, 'd': 8}

_tokenname_to_initialiser: Dict[str, str] = {'hex': 'hex', '0x': 'hex', '0X': 'hex', 'oct': 'oct', '0o': 'oct',
                                             '0O': 'oct', 'bin': 'bin', '0b': 'bin', '0B': 'bin', 'bits': 'bits',
                                             'bytes': 'bytes', 'pad': 'pad', 'bfloat': 'bfloat',
                                             'float8_143': 'float8_143', 'float8_152': 'float8_152'}


def _str_to_bitstore(s: str, _str_to_bitstore_cache={}) -> BitStore:
    try:
        return _str_to_bitstore_cache[s]
    except KeyError:
        try:
            _, tokens = tokenparser(s)
        except ValueError as e:
            raise CreationError(*e.args)
        bs = BitStore()
        if tokens:
            bs = bs + _bitstore_from_token(*tokens[0])
            for token in tokens[1:]:
                bs = bs + _bitstore_from_token(*token)
        bs.immutable = True
        _str_to_bitstore_cache[s] = bs
        if len(_str_to_bitstore_cache) > CACHE_SIZE:
            # Remove the oldest one. FIFO.
            del _str_to_bitstore_cache[next(iter(_str_to_bitstore_cache))]
        return bs


def _bin2bitstore(binstring: str) -> BitStore:
    binstring = tidy_input_string(binstring)
    binstring = binstring.replace('0b', '')
    return _bin2bitstore_unsafe(binstring)


def _bin2bitstore_unsafe(binstring: str) -> BitStore:
    try:
        return BitStore(binstring)
    except ValueError:
        raise CreationError(f"Invalid character in bin initialiser {binstring}.")


def _hex2bitstore(hexstring: str) -> BitStore:
    hexstring = tidy_input_string(hexstring)
    hexstring = hexstring.replace('0x', '')
    try:
        ba = bitarray.util.hex2ba(hexstring)
    except ValueError:
        raise CreationError("Invalid symbol in hex initialiser.")
    return BitStore(ba)


def _oct2bitstore(octstring: str) -> BitStore:
    octstring = tidy_input_string(octstring)
    octstring = octstring.replace('0o', '')
    try:
        ba = bitarray.util.base2ba(8, octstring)
    except ValueError:
        raise CreationError("Invalid symbol in oct initialiser.")
    return BitStore(ba)


def _ue2bitstore(i: Union[str, int]) -> BitStore:
    i = int(i)
    if _lsb0:
        raise CreationError("Exp-Golomb codes cannot be used in lsb0 mode.")
    if i < 0:
        raise CreationError("Cannot use negative initialiser for unsigned exponential-Golomb.")
    if i == 0:
        return BitStore('1')
    tmp = i + 1
    leadingzeros = -1
    while tmp > 0:
        tmp >>= 1
        leadingzeros += 1
    remainingpart = i + 1 - (1 << leadingzeros)
    return BitStore('0' * leadingzeros + '1') + _uint2bitstore(remainingpart, leadingzeros)


def _se2bitstore(i: Union[str, int]) -> BitStore:
    i = int(i)
    if i > 0:
        u = (i * 2) - 1
    else:
        u = -2 * i
    return _ue2bitstore(u)


def _uie2bitstore(i: Union[str, int]) -> BitStore:
    if _lsb0:
        raise CreationError("Exp-Golomb codes cannot be used in lsb0 mode.")
    i = int(i)
    if i < 0:
        raise CreationError("Cannot use negative initialiser for unsigned interleaved exponential-Golomb.")
    return BitStore('1' if i == 0 else '0' + '0'.join(bin(i + 1)[3:]) + '1')


def _sie2bitstore(i: Union[str, int]) -> BitStore:
    i = int(i)
    if _lsb0:
        raise CreationError("Exp-Golomb codes cannot be used in lsb0 mode.")
    if i == 0:
        return BitStore('1')
    else:
        return _uie2bitstore(abs(i)) + (BitStore('1') if i < 0 else BitStore('0'))


def _bfloat2bitstore(f: Union[str, float]) -> BitStore:
    f = float(f)
    try:
        b = struct.pack('>f', f)
    except OverflowError:
        # For consistency we overflow to 'inf'.
        b = struct.pack('>f', float('inf') if f > 0 else float('-inf'))
    return BitStore(frombytes=b[0:2])


def _bfloatle2bitstore(f: Union[str, float]) -> BitStore:
    f = float(f)
    try:
        b = struct.pack('<f', f)
    except OverflowError:
        # For consistency we overflow to 'inf'.
        b = struct.pack('<f', float('inf') if f > 0 else float('-inf'))
    return BitStore(frombytes=b[2:4])


def _float8_143_2bitstore(f: Union[str, float]) -> BitStore:
    f = float(f)
    u = fp143_fmt.float_to_int8(f)
    return _uint2bitstore(u, 8)


def _float8_152_2bitstore(f: Union[str, float]) -> BitStore:
    f = float(f)
    u = fp152_fmt.float_to_int8(f)
    return _uint2bitstore(u, 8)


def _uint2bitstore(uint: Union[str, int], length: int) -> BitStore:
    uint = int(uint)
    try:
        x = BitStore(bitarray.util.int2ba(uint, length=length, endian='big', signed=False))
    except OverflowError as e:
        if uint >= (1 << length):
            msg = f"{uint} is too large an unsigned integer for a bitstring of length {length}. " \
                  f"The allowed range is [0, {(1 << length) - 1}]."
            raise CreationError(msg)
        if uint < 0:
            raise CreationError("uint cannot be initialised with a negative number.")
        raise e
    return x


def _int2bitstore(i: Union[str, int], length: int) -> BitStore:
    i = int(i)
    try:
        x = BitStore(bitarray.util.int2ba(i, length=length, endian='big', signed=True))
    except OverflowError as e:
        if i >= (1 << (length - 1)) or i < -(1 << (length - 1)):
            raise CreationError(f"{i} is too large a signed integer for a bitstring of length {length}. "
                                f"The allowed range is [{-(1 << (length - 1))}, {(1 << (length - 1)) - 1}].")
        else:
            raise e
    return x


def _uintbe2bitstore(i: Union[str, int], length: int) -> BitStore:
    if length % 8 != 0:
        raise CreationError(f"Big-endian integers must be whole-byte. Length = {length} bits.")
    return _uint2bitstore(i, length)


def _intbe2bitstore(i: int, length: int) -> BitStore:
    if length % 8 != 0:
        raise CreationError(f"Big-endian integers must be whole-byte. Length = {length} bits.")
    return _int2bitstore(i, length)


def _uintle2bitstore(i: int, length: int) -> BitStore:
    if length % 8 != 0:
        raise CreationError(f"Little-endian integers must be whole-byte. Length = {length} bits.")
    x = _uint2bitstore(i, length).tobytes()
    return BitStore(frombytes=x[::-1])


def _intle2bitstore(i: int, length: int) -> BitStore:
    if length % 8 != 0:
        raise CreationError(f"Little-endian integers must be whole-byte. Length = {length} bits.")
    x = _int2bitstore(i, length).tobytes()
    return BitStore(frombytes=x[::-1])


def _float2bitstore(f: Union[str, float], length: int) -> BitStore:
    f = float(f)
    try:
        fmt = {16: '>e', 32: '>f', 64: '>d'}[length]
    except KeyError:
        raise InterpretError(f"Floats can only be 16, 32 or 64 bits long, not {length} bits")
    try:
        b = struct.pack(fmt, f)
        assert len(b) * 8 == length
    except (OverflowError, struct.error) as e:
        # If float64 doesn't fit it automatically goes to 'inf'. This reproduces that behaviour for other types.
        if length in [16, 32]:
            b = struct.pack(fmt, float('inf') if f > 0 else float('-inf'))
        else:
            raise e
    return BitStore(frombytes=b)


def _floatle2bitstore(f: Union[str, float], length: int) -> BitStore:
    f = float(f)
    try:
        fmt = {16: '<e', 32: '<f', 64: '<d'}[length]
    except KeyError:
        raise InterpretError(f"Floats can only be 16, 32 or 64 bits long, not {length} bits")
    try:
        b = struct.pack(fmt, f)
        assert len(b) * 8 == length
    except (OverflowError, struct.error) as e:
        # If float64 doesn't fit it automatically goes to 'inf'. This reproduces that behaviour for other types.
        if length in [16, 32]:
            b = struct.pack(fmt, float('inf') if f > 0 else float('-inf'))
        else:
            raise e
    return BitStore(frombytes=b)


# Create native-endian functions as aliases depending on the byteorder
if byteorder == 'little':
    _uintne2bitstore = _uintle2bitstore
    _intne2bitstore = _intle2bitstore
    _bfloatne2bitstore = _bfloatle2bitstore
    _floatne2bitstore = _floatle2bitstore
else:
    _uintne2bitstore = _uintbe2bitstore
    _intne2bitstore = _intbe2bitstore
    _bfloatne2bitstore = _bfloat2bitstore
    _floatne2bitstore = _float2bitstore

# Given a string of the format 'name=value' get a bitstore representing it by using
# _name2bitstore_func[name](value)
_name2bitstore_func: Dict[str, Callable[..., BitStore]] = {
    'hex': _hex2bitstore,
    '0x': _hex2bitstore,
    '0X': _hex2bitstore,
    'bin': _bin2bitstore,
    '0b': _bin2bitstore,
    '0B': _bin2bitstore,
    'oct': _oct2bitstore,
    '0o': _oct2bitstore,
    '0O': _oct2bitstore,
    'se': _se2bitstore,
    'ue': _ue2bitstore,
    'sie': _sie2bitstore,
    'uie': _uie2bitstore,
    'bfloat': _bfloat2bitstore,
    'bfloatbe': _bfloat2bitstore,
    'bfloatle': _bfloatle2bitstore,
    'bfloatne': _bfloatne2bitstore,
    'float8_143': _float8_143_2bitstore,
    'float8_152': _float8_152_2bitstore,
}

# Given a string of the format 'name[:]length=value' get a bitstore representing it by using
# _name2bitstore_func_with_length[name](value, length)
_name2bitstore_func_with_length: Dict[str, Callable[..., BitStore]] = {
    'uint': _uint2bitstore,
    'int': _int2bitstore,
    'uintbe': _uintbe2bitstore,
    'intbe': _intbe2bitstore,
    'uintle': _uintle2bitstore,
    'intle': _intle2bitstore,
    'uintne': _uintne2bitstore,
    'intne': _intne2bitstore,
    'float': _float2bitstore,
    'floatbe': _float2bitstore,  # same as 'float'
    'floatle': _floatle2bitstore,
    'floatne': _floatne2bitstore,
}


def _bitstore_from_token(name: str, token_length: Optional[int], value: Optional[str]) -> BitStore:
    if token_length == 0:
        return BitStore()
    # For pad token just return the length in zero bits
    if name == 'pad':
        bs = BitStore(token_length)
        bs.setall(0)
        return bs
    if value is None:
        if token_length is None:
            raise ValueError(f"Token has no value ({name}=???).")
        else:
            raise ValueError(f"Token has no value ({name}:{token_length}=???).")

    if name in _name2bitstore_func:
        bs = _name2bitstore_func[name](value)
    elif name in _name2bitstore_func_with_length:
        bs = _name2bitstore_func_with_length[name](value, token_length)
    elif name == 'bool':
        if value in (1, 'True', '1'):
            bs = BitStore('1')
        elif value in (0, 'False', '0'):
            bs = BitStore('0')
        else:
            raise CreationError("bool token can only be 'True' or 'False'.")
    else:
        raise CreationError(f"Can't parse token name {name}.")
    if token_length is not None and len(bs) != token_length:
        raise CreationError(f"Token with length {token_length} packed with value of length {len(bs)} "
                            f"({name}:{token_length}={value}).")
    return bs


class Bits:
    """A container holding an immutable sequence of bits.

    For a mutable container use the BitArray class instead.

    Methods:

    all() -- Check if all specified bits are set to 1 or 0.
    any() -- Check if any of specified bits are set to 1 or 0.
    copy() - Return a copy of the bitstring.
    count() -- Count the number of bits set to 1 or 0.
    cut() -- Create generator of constant sized chunks.
    endswith() -- Return whether the bitstring ends with a sub-string.
    find() -- Find a sub-bitstring in the current bitstring.
    findall() -- Find all occurrences of a sub-bitstring in the current bitstring.
    join() -- Join bitstrings together using current bitstring.
    pp() -- Pretty print the bitstring.
    rfind() -- Seek backwards to find a sub-bitstring.
    split() -- Create generator of chunks split by a delimiter.
    startswith() -- Return whether the bitstring starts with a sub-bitstring.
    tobitarray() -- Return bitstring as a bitarray from the bitarray package.
    tobytes() -- Return bitstring as bytes, padding if needed.
    tofile() -- Write bitstring to file, padding if needed.
    unpack() -- Interpret bits using format string.

    Special methods:

    Also available are the operators [], ==, !=, +, *, ~, <<, >>, &, |, ^.

    Properties:

    bin -- The bitstring as a binary string.
    hex -- The bitstring as a hexadecimal string.
    oct -- The bitstring as an octal string.
    bytes -- The bitstring as a bytes object.
    int -- Interpret as a two's complement signed integer.
    uint -- Interpret as a two's complement unsigned integer.
    float / floatbe -- Interpret as a big-endian floating point number.
    bool -- For single bit bitstrings, interpret as True or False.
    se -- Interpret as a signed exponential-Golomb code.
    ue -- Interpret as an unsigned exponential-Golomb code.
    sie -- Interpret as a signed interleaved exponential-Golomb code.
    uie -- Interpret as an unsigned interleaved exponential-Golomb code.
    floatle -- Interpret as a little-endian floating point number.
    floatne -- Interpret as a native-endian floating point number.
    bfloat / bfloatbe -- Interpret as a big-endian 16-bit bfloat type.
    bfloatle -- Interpret as a little-endian 16-bit bfloat type.
    bfloatne -- Interpret as a native-endian 16-bit bfloat type.
    intbe -- Interpret as a big-endian signed integer.
    intle -- Interpret as a little-endian signed integer.
    intne -- Interpret as a native-endian signed integer.
    uintbe -- Interpret as a big-endian unsigned integer.
    uintle -- Interpret as a little-endian unsigned integer.
    uintne -- Interpret as a native-endian unsigned integer.

    len -- Length of the bitstring in bits.

    """

    @classmethod
    def _setlsb0methods(cls, lsb0: bool) -> None:
        if lsb0:
            cls._find = cls._find_lsb0  # type: ignore
            cls._rfind = cls._rfind_lsb0  # type: ignore
            cls._findall = cls._findall_lsb0  # type: ignore
        else:
            cls._find = cls._find_msb0  # type: ignore
            cls._rfind = cls._rfind_msb0  # type: ignore
            cls._findall = cls._findall_msb0  # type: ignore

    __slots__ = ('_bitstore')

    # Creates dictionaries to quickly reverse single bytes
    _int8ReversalDict: Dict[int, int] = {i: int("{0:08b}".format(i)[::-1], 2) for i in range(0x100)}
    _byteReversalDict: Dict[int, bytes] = {i: bytes([int("{0:08b}".format(i)[::-1], 2)]) for i in range(0x100)}

    def __init__(self, __auto: Optional[Union[BitsType, int]] = None, length: Optional[int] = None,
                 offset: Optional[int] = None, **kwargs) -> None:
        """Either specify an 'auto' initialiser:
        A string of comma separated tokens, an integer, a file object,
        a bytearray, a boolean iterable, an array or another bitstring.

        Or initialise via **kwargs with one (and only one) of:
        bin -- binary string representation, e.g. '0b001010'.
        hex -- hexadecimal string representation, e.g. '0x2ef'
        oct -- octal string representation, e.g. '0o777'.
        bytes -- raw data as a bytes object, for example read from a binary file.
        int -- a signed integer.
        uint -- an unsigned integer.
        float / floatbe -- a big-endian floating point number.
        bool -- a boolean (True or False).
        se -- a signed exponential-Golomb code.
        ue -- an unsigned exponential-Golomb code.
        sie -- a signed interleaved exponential-Golomb code.
        uie -- an unsigned interleaved exponential-Golomb code.
        floatle -- a little-endian floating point number.
        floatne -- a native-endian floating point number.
        bfloat / bfloatbe - a big-endian bfloat format 16-bit floating point number.
        bfloatle -- a little-endian bfloat format 16-bit floating point number.
        bfloatne -- a native-endian bfloat format 16-bit floating point number.
        intbe -- a signed big-endian whole byte integer.
        intle -- a signed little-endian whole byte integer.
        intne -- a signed native-endian whole byte integer.
        uintbe -- an unsigned big-endian whole byte integer.
        uintle -- an unsigned little-endian whole byte integer.
        uintne -- an unsigned native-endian whole byte integer.
        filename -- the path of a file which will be opened in binary read-only mode.

        Other keyword arguments:
        length -- length of the bitstring in bits, if needed and appropriate.
                  It must be supplied for all integer and float initialisers.
        offset -- bit offset to the data. These offset bits are
                  ignored and this is mainly intended for use when
                  initialising using 'bytes' or 'filename'.

        """
        self._bitstore.immutable = True

    def __new__(cls: Type[TBits], __auto: Optional[Union[BitsType, int]] = None, length: Optional[int] = None,
                offset: Optional[int] = None, pos: Optional[int] = None, **kwargs) -> TBits:
        x = object.__new__(cls)
        if __auto is None and not kwargs:
            # No initialiser so fill with zero bits up to length
            if length is not None:
                x._bitstore = BitStore(length)
                x._bitstore.setall(0)
            else:
                x._bitstore = BitStore()
            return x
        x._initialise(__auto, length, offset, **kwargs)
        return x

    @classmethod
    def _create_from_bitstype(cls: Type[TBits], auto: Optional[BitsType]) -> TBits:
        b = cls()
        if auto is None:
            return b
        if isinstance(auto, numbers.Integral):
            raise TypeError(f"It's no longer possible to auto initialise a bitstring from an integer."
                            f" Use '{cls.__name__}({auto})' instead of just '{auto}' as this makes it "
                            f"clearer that a bitstring of {int(auto)} zero bits will be created.")
        b._setauto(auto, None, None)
        return b

    def _initialise(self, __auto: Any, length: Optional[int], offset: Optional[int], **kwargs) -> None:
        if length is not None and length < 0:
            raise CreationError("bitstring length cannot be negative.")
        if offset is not None and offset < 0:
            raise CreationError("offset must be >= 0.")
        if __auto is not None:
            self._setauto(__auto, length, offset)
            return
        k, v = kwargs.popitem()
        try:
            setting_function = self._setfunc[k]
        except KeyError:
            if k == 'auto':
                raise CreationError(f"The 'auto' parameter should not be given explicitly - just use the first positional argument. "
                                    f"Instead of '{self.__class__.__name__}(auto=x)' use '{self.__class__.__name__}(x)'.")
            else:
                raise CreationError(f"Unrecognised keyword '{k}' used to initialise.")
        setting_function(self, v, length, offset)

    def __getattr__(self, attribute: str) -> Any:
        # Support for arbitrary attributes like u16 or f64.
        letter_to_getter: Dict[str, Callable[..., Union[int, float, str]]] = \
            {'u': self._getuint,
             'i': self._getint,
             'f': self._getfloatbe,
             'b': self._getbin,
             'o': self._getoct,
             'h': self._gethex}
        short_token: Pattern[str] = re.compile(r'^(?P<name>[uifboh]):?(?P<len>\d+)$', re.IGNORECASE)
        m1_short = short_token.match(attribute)
        if m1_short:
            length = int(m1_short.group('len'))
            if length is not None and self.len != length:
                raise InterpretError(f"bitstring length {self.len} doesn't match length of property {attribute}.")
            name = m1_short.group('name')
            f = letter_to_getter[name]
            return f()
        # Try to split into [name][length], then try standard properties
        name_length_pattern: Pattern[str] = re.compile(r'^(?P<name>[a-z]+):?(?P<len>\d+)$', re.IGNORECASE)
        name_length = name_length_pattern.match(attribute)
        if name_length:
            name = name_length.group('name')
            length = int(name_length.group('len'))
            if name == 'bytes' and length is not None:
                length *= 8
            if length is not None and self.len != int(length):
                raise InterpretError(f"bitstring length {self.len} doesn't match length of property {attribute}.")
            try:
                return getattr(self, name)
            except AttributeError:
                pass
        raise AttributeError(f"'{self.__class__.__name__}' object has no attribute '{attribute}'.")

    def __iter__(self) -> Iterable[bool]:
        return iter(self._bitstore)

    def __copy__(self: TBits) -> TBits:
        """Return a new copy of the Bits for the copy module."""
        # Note that if you want a new copy (different ID), use _copy instead.
        # The copy can return self as it's immutable.
        return self

    def __lt__(self, other: Any) -> bool:
        # bitstrings can't really be ordered.
        return NotImplemented

    def __gt__(self, other: Any) -> bool:
        return NotImplemented

    def __le__(self, other: Any) -> bool:
        return NotImplemented

    def __ge__(self, other: Any) -> bool:
        return NotImplemented

    def __add__(self: TBits, bs: BitsType) -> TBits:
        """Concatenate bitstrings and return new bitstring.

        bs -- the bitstring to append.

        """
        bs = self.__class__._create_from_bitstype(bs)
        if bs.len <= self.len:
            s = self._copy()
            s._addright(bs)
        else:
            s = bs._copy()
            s = self.__class__(s)
            s._addleft(self)
        return s

    def __radd__(self: TBits, bs: BitsType) -> TBits:
        """Append current bitstring to bs and return new bitstring.

        bs -- An object that can be 'auto' initialised as a bitstring that will be appended to.

        """
        bs = self.__class__._create_from_bitstype(bs)
        return bs.__add__(self)

    @overload
    def __getitem__(self: TBits, key: slice) -> TBits:
        ...

    @overload
    def __getitem__(self, key: int) -> bool:
        ...

    def __getitem__(self: TBits, key: Union[slice, int]) -> Union[TBits, bool]:
        """Return a new bitstring representing a slice of the current bitstring.

        Indices are in units of the step parameter (default 1 bit).
        Stepping is used to specify the number of bits in each item.

        >>> print(BitArray('0b00110')[1:4])
        '0b011'
        >>> print(BitArray('0x00112233')[1:3:8])
        '0x1122'

        """
        if isinstance(key, numbers.Integral):
            return bool(self._bitstore.getindex(key))
        x = self._bitstore.getslice(key)
        bs = self.__class__()
        bs._bitstore = x
        return bs

    def __len__(self) -> int:
        """Return the length of the bitstring in bits."""
        return self._getlength()

    def __bytes__(self) -> bytes:
        return self.tobytes()

    def __str__(self) -> str:
        """Return approximate string representation of bitstring for printing.

        Short strings will be given wholly in hexadecimal or binary. Longer
        strings may be part hexadecimal and part binary. Very long strings will
        be truncated with '...'.

        """
        length = self.len
        if not length:
            return ''
        if length > MAX_CHARS * 4:
            # Too long for hex. Truncate...
            return ''.join(('0x', self._readhex(0, MAX_CHARS * 4), '...'))
        # If it's quite short and we can't do hex then use bin
        if length < 32 and length % 4 != 0:
            return '0b' + self.bin
        # If we can use hex then do so
        if not length % 4:
            return '0x' + self.hex
        # Otherwise first we do as much as we can in hex
        # then add on 1, 2 or 3 bits on at the end
        bits_at_end = length % 4
        return ''.join(('0x', self._readhex(0, length - bits_at_end),
                        ', ', '0b',
                        self._readbin(length - bits_at_end, bits_at_end)))

    def _repr(self, classname: str, length: int, offset: int, filename: str, pos: int):
        pos_string = f', pos={pos}' if pos else ''
        if filename:
            offsetstring = f', offset={offset}' if offset else ''
            return f"{classname}(filename={repr(filename)}, length={length}{offsetstring}{pos_string})"
        else:
            s = self.__str__()
            lengthstring = ''
            if s.endswith('...'):
                lengthstring = f'  # length={length}'
            return f"{classname}('{s}'{pos_string}){lengthstring}"

    def __repr__(self) -> str:
        """Return representation that could be used to recreate the bitstring.

        If the returned string is too long it will be truncated. See __str__().

        """
        return self._repr(self.__class__.__name__, len(self), self._bitstore.offset, self._bitstore.filename, 0)

    def __eq__(self, bs: Any) -> bool:
        """Return True if two bitstrings have the same binary representation.

        >>> BitArray('0b1110') == '0xe'
        True

        """
        try:
            bs = Bits._create_from_bitstype(bs)
        except TypeError:
            return False
        return self._bitstore == bs._bitstore

    def __ne__(self, bs: Any) -> bool:
        """Return False if two bitstrings have the same binary representation.

        >>> BitArray('0b111') == '0x7'
        False

        """
        return not self.__eq__(bs)

    def __invert__(self: TBits) -> TBits:
        """Return bitstring with every bit inverted.

        Raises Error if the bitstring is empty.

        """
        if not self.len:
            raise Error("Cannot invert empty bitstring.")
        s = self._copy()
        s._invert_all()
        return s

    def __lshift__(self: TBits, n: int) -> TBits:
        """Return bitstring with bits shifted by n to the left.

        n -- the number of bits to shift. Must be >= 0.

        """
        if n < 0:
            raise ValueError("Cannot shift by a negative amount.")
        if not self.len:
            raise ValueError("Cannot shift an empty bitstring.")
        n = min(n, self.len)
        s = self._absolute_slice(n, self.len)
        s._addright(Bits(n))
        return s

    def __rshift__(self: TBits, n: int) -> TBits:
        """Return bitstring with bits shifted by n to the right.

        n -- the number of bits to shift. Must be >= 0.

        """
        if n < 0:
            raise ValueError("Cannot shift by a negative amount.")
        if not self.len:
            raise ValueError("Cannot shift an empty bitstring.")
        if not n:
            return self._copy()
        s = self.__class__(length=min(n, self.len))
        n = min(n, self.len)
        s._addright(self._absolute_slice(0, self.len - n))
        return s

    def __mul__(self: TBits, n: int) -> TBits:
        """Return bitstring consisting of n concatenations of self.

        Called for expression of the form 'a = b*3'.
        n -- The number of concatenations. Must be >= 0.

        """
        if n < 0:
            raise ValueError("Cannot multiply by a negative integer.")
        if not n:
            return self.__class__()
        s = self._copy()
        s._imul(n)
        return s

    def __rmul__(self: TBits, n: int) -> TBits:
        """Return bitstring consisting of n concatenations of self.

        Called for expressions of the form 'a = 3*b'.
        n -- The number of concatenations. Must be >= 0.

        """
        return self.__mul__(n)

    def __and__(self: TBits, bs: BitsType) -> TBits:
        """Bit-wise 'and' between two bitstrings. Returns new bitstring.

        bs -- The bitstring to '&' with.

        Raises ValueError if the two bitstrings have differing lengths.

        """
        bs = Bits._create_from_bitstype(bs)
        if self.len != bs.len:
            raise ValueError("Bitstrings must have the same length for & operator.")
        s = self._copy()
        s._bitstore &= bs._bitstore
        return s

    def __rand__(self: TBits, bs: BitsType) -> TBits:
        """Bit-wise 'and' between two bitstrings. Returns new bitstring.

        bs -- the bitstring to '&' with.

        Raises ValueError if the two bitstrings have differing lengths.

        """
        return self.__and__(bs)

    def __or__(self: TBits, bs: BitsType) -> TBits:
        """Bit-wise 'or' between two bitstrings. Returns new bitstring.

        bs -- The bitstring to '|' with.

        Raises ValueError if the two bitstrings have differing lengths.

        """
        bs = Bits._create_from_bitstype(bs)
        if self.len != bs.len:
            raise ValueError("Bitstrings must have the same length for | operator.")
        s = self._copy()
        s._bitstore |= bs._bitstore
        return s

    def __ror__(self: TBits, bs: BitsType) -> TBits:
        """Bit-wise 'or' between two bitstrings. Returns new bitstring.

        bs -- The bitstring to '|' with.

        Raises ValueError if the two bitstrings have differing lengths.

        """
        return self.__or__(bs)

    def __xor__(self: TBits, bs: BitsType) -> TBits:
        """Bit-wise 'xor' between two bitstrings. Returns new bitstring.

        bs -- The bitstring to '^' with.

        Raises ValueError if the two bitstrings have differing lengths.

        """
        bs = Bits._create_from_bitstype(bs)
        if self.len != bs.len:
            raise ValueError("Bitstrings must have the same length for ^ operator.")
        s = self._copy()
        s._bitstore ^= bs._bitstore
        return s

    def __rxor__(self: TBits, bs: BitsType) -> TBits:
        """Bit-wise 'xor' between two bitstrings. Returns new bitstring.

        bs -- The bitstring to '^' with.

        Raises ValueError if the two bitstrings have differing lengths.

        """
        return self.__xor__(bs)

    def __contains__(self, bs: BitsType) -> bool:
        """Return whether bs is contained in the current bitstring.

        bs -- The bitstring to search for.

        """
        found = Bits.find(self, bs, bytealigned=False)
        return bool(found)

    def __hash__(self) -> int:
        """Return an integer hash of the object."""
        # Only requirement is that equal bitstring should return the same hash.
        # For equal bitstrings the bytes at the start/end will be the same and they will have the same length
        # (need to check the length as there could be zero padding when getting the bytes). We do not check any
        # bit position inside the bitstring as that does not feature in the __eq__ operation.
        if self.len <= 2000:
            # Use the whole bitstring.
            return hash((self.tobytes(), self.len))
        else:
            # We can't in general hash the whole bitstring (it could take hours!)
            # So instead take some bits from the start and end.
            return hash(((self[:800] + self[-800:]).tobytes(), self.len))

    def __bool__(self) -> bool:
        """Return True if any bits are set to 1, otherwise return False."""
        return len(self) != 0

    @classmethod
    def _init_with_token(cls: Type[TBits], name: str, token_length: Optional[int], value: Optional[str]) -> TBits:
        if token_length == 0:
            return cls()
        # For pad token just return the length in zero bits
        if name == 'pad':
            return cls(token_length)
        if value is None:
            if token_length is None:
                raise ValueError(f"Token has no value ({name}=???).")
            else:
                raise ValueError(f"Token has no value ({name}:{token_length}=???).")
        try:
            b = cls(**{_tokenname_to_initialiser[name]: value})
        except KeyError:
            if name in ('se', 'ue', 'sie', 'uie'):
                if _lsb0:
                    raise CreationError("Exp-Golomb codes cannot be used in lsb0 mode.")
                b = cls(**{name: int(value)})
            elif name in ('uint', 'int', 'uintbe', 'intbe', 'uintle', 'intle', 'uintne', 'intne'):
                b = cls(**{name: int(value), 'length': token_length})
            elif name in ('float', 'floatbe', 'floatle', 'floatne'):
                b = cls(**{name: float(value), 'length': token_length})
            elif name == 'bool':
                if value in (1, 'True', '1'):
                    b = cls(bool=True)
                elif value in (0, 'False', '0'):
                    b = cls(bool=False)
                else:
                    raise CreationError("bool token can only be 'True' or 'False'.")
            else:
                raise CreationError(f"Can't parse token name {name}.")
        if token_length is not None and b.len != token_length:
            raise CreationError(f"Token with length {token_length} packed with value of length {b.len} "
                                f"({name}:{token_length}={value}).")
        return b

    def _clear(self) -> None:
        """Reset the bitstring to an empty state."""
        self._bitstore = BitStore()

    def _setauto(self, s: Union[BitsType, int], length: Optional[int], offset: Optional[int]) -> None:
        """Set bitstring from a bitstring, file, bool, integer, array, iterable or string."""
        # As s can be so many different things it's important to do the checks
        # in the correct order, as some types are also other allowed types.
        # So str must be checked before Iterable
        # and bytes/bytearray before Iterable but after str!
        if offset is None:
            offset = 0
        if isinstance(s, Bits):
            if length is None:
                length = s._getlength() - offset
            self._bitstore = s._bitstore.getslice(slice(offset, offset + length, None))
            return

        if isinstance(s, io.BytesIO):
            if length is None:
                length = s.seek(0, 2) * 8 - offset
            byteoffset, offset = divmod(offset, 8)
            bytelength = (length + byteoffset * 8 + offset + 7) // 8 - byteoffset
            if length + byteoffset * 8 + offset > s.seek(0, 2) * 8:
                raise CreationError("BytesIO object is not long enough for specified length and offset.")
            self._bitstore = BitStore(frombytes=s.getvalue()[byteoffset: byteoffset + bytelength]).getslice(
                slice(offset, offset + length))
            return

        if isinstance(s, io.BufferedReader):
            m = mmap.mmap(s.fileno(), 0, access=mmap.ACCESS_READ)
            self._bitstore = BitStore(buffer=m, offset=offset, length=length, filename=s.name, immutable=True)
            return

        if isinstance(s, bitarray.bitarray):
            if length is None:
                if offset > len(s):
                    raise CreationError(f"Offset of {offset} too large for bitarray of length {len(s)}.")
                self._bitstore = BitStore(s[offset:])
            else:
                if offset + length > len(s):
                    raise CreationError(
                        f"Offset of {offset} and length of {length} too large for bitarray of length {len(s)}.")
                self._bitstore = BitStore(s[offset: offset + length])
            return

        if length is not None:
            raise CreationError("The length keyword isn't applicable to this initialiser.")
        if offset > 0:
            raise CreationError("The offset keyword isn't applicable to this initialiser.")
        if isinstance(s, str):
            self._bitstore = _str_to_bitstore(s)
            return
        if isinstance(s, (bytes, bytearray, memoryview)):
            self._bitstore = BitStore(frombytes=bytearray(s))
            return
        if isinstance(s, array.array):
            self._bitstore = BitStore(frombytes=bytearray(s.tobytes()))
            return
        if isinstance(s, numbers.Integral):
            # Initialise with s zero bits.
            if s < 0:
                raise CreationError(f"Can't create bitstring of negative length {s}.")
            self._bitstore = BitStore(int(s))
            self._bitstore.setall(0)
            return
        if isinstance(s, abc.Iterable):
            # Evaluate each item as True or False and set bits to 1 or 0.
            self._setbin_unsafe(''.join(str(int(bool(x))) for x in s))
            return
        raise TypeError(f"Cannot initialise bitstring from {type(s)}.")

    def _setfile(self, filename: str, length: Optional[int], offset: Optional[int]) -> None:
        """Use file as source of bits."""
        with open(pathlib.Path(filename), 'rb') as source:
            if offset is None:
                offset = 0
            m = mmap.mmap(source.fileno(), 0, access=mmap.ACCESS_READ)
            self._bitstore = BitStore(buffer=m, offset=offset, length=length, filename=source.name, immutable=True)

    def _setbitarray(self, ba: bitarray.bitarray, length: Optional[int], offset: Optional[int]) -> None:
        if offset is None:
            offset = 0
        if offset > len(ba):
            raise CreationError(f"Offset of {offset} too large for bitarray of length {len(ba)}.")
        if length is None:
            self._bitstore = BitStore(ba[offset:])
        else:
            if offset + length > len(ba):
                raise CreationError(
                    f"Offset of {offset} and length of {length} too large for bitarray of length {len(ba)}.")
            self._bitstore = BitStore(ba[offset: offset + length])

    def _setbits(self, bs: BitsType, length: None = None, offset: None = None) -> None:
        bs = Bits._create_from_bitstype(bs)
        self._bitstore = bs._bitstore

    def _setfloat152(self, f: float, length: None = None, _offset: None = None):
        u = fp152_fmt.float_to_int8(f)
        self._bitstore = _uint2bitstore(u, 8)

    def _setfloat143(self, f: float, length: None = None, _offset: None = None):
        u = fp143_fmt.float_to_int8(f)
        self._bitstore = _uint2bitstore(u, 8)

    def _setbytes(self, data: Union[bytearray, bytes],
                  length: Optional[int] = None, offset: Optional[int] = None) -> None:
        """Set the data from a bytes or bytearray object."""
        if offset is None and length is None:
            self._bitstore = BitStore(frombytes=bytearray(data))
            return
        data = bytearray(data)
        if offset is None:
            offset = 0
        if length is None:
            # Use to the end of the data
            length = len(data) * 8 - offset
        else:
            if length + offset > len(data) * 8:
                raise CreationError(f"Not enough data present. Need {length + offset} bits, have {len(data) * 8}.")
        self._bitstore = BitStore(buffer=data).getslice_msb0(slice(offset, offset + length, None))

    def _readbytes(self, start: int, length: int) -> bytes:
        """Read bytes and return them. Note that length is in bits."""
        assert length % 8 == 0
        assert start + length <= self.len
        return self._bitstore.getslice(slice(start, start + length, None)).tobytes()

    def _getbytes(self) -> bytes:
        """Return the data as an ordinary bytes object."""
        if self.len % 8:
            raise InterpretError("Cannot interpret as bytes unambiguously - not multiple of 8 bits.")
        return self._readbytes(0, self.len)

    _unprintable = list(range(0x00, 0x20))  # ASCII control characters
    _unprintable.extend(range(0x7f, 0xff))  # DEL char + non-ASCII

    def _getbytes_printable(self) -> str:
        """Return an approximation of the data as a string of printable characters."""
        bytes_ = self._getbytes()
        # For everything that isn't printable ASCII, use value from 'Latin Extended-A' unicode block.
        string = ''.join(chr(0x100 + x) if x in Bits._unprintable else chr(x) for x in bytes_)
        return string

    def _setuint(self, uint: int, length: Optional[int] = None, _offset: None = None) -> None:
        """Reset the bitstring to have given unsigned int interpretation."""
        # If no length given, and we've previously been given a length, use it.
        if length is None and hasattr(self, 'len') and self.len != 0:
            length = self.len
        if length is None or length == 0:
            raise CreationError("A non-zero length must be specified with a uint initialiser.")
        if _offset is not None:
            raise CreationError("An offset can't be specified with an integer initialiser.")
        self._bitstore = _uint2bitstore(uint, length)

    def _readuint(self, start: int, length: int) -> int:
        """Read bits and interpret as an unsigned int."""
        if length == 0:
            raise InterpretError("Cannot interpret a zero length bitstring as an integer.")
        ip = bitarray.util.ba2int(self._bitstore.getslice(slice(start, start + length, None)), signed=False)
        return ip

    def _getuint(self) -> int:
        """Return data as an unsigned int."""
        if self.len == 0:
            raise InterpretError("Cannot interpret a zero length bitstring as an integer.")
        bs = self._bitstore.copy() if self._bitstore.modified else self._bitstore
        return bitarray.util.ba2int(bs, signed=False)

    def _setint(self, int_: int, length: Optional[int] = None, _offset: None = None) -> None:
        """Reset the bitstring to have given signed int interpretation."""
        # If no length given, and we've previously been given a length, use it.
        if length is None and hasattr(self, 'len') and self.len != 0:
            length = self.len
        if length is None or length == 0:
            raise CreationError("A non-zero length must be specified with an int initialiser.")
        if _offset is not None:
            raise CreationError("An offset can't be specified with an integer initialiser.")
        self._bitstore = _int2bitstore(int_, length)

    def _readint(self, start: int, length: int) -> int:
        """Read bits and interpret as a signed int"""
        if length == 0:
            raise InterpretError("Cannot interpret a zero length bitstring as an integer.")
        ip = bitarray.util.ba2int(self._bitstore.getslice(slice(start, start + length, None)), signed=True)
        return ip

    def _getint(self) -> int:
        """Return data as a two's complement signed int."""
        if self.len == 0:
            raise InterpretError("Cannot interpret a zero length bitstring as an integer.")
        bs = self._bitstore.copy() if self._bitstore.modified else self._bitstore
        return bitarray.util.ba2int(bs, signed=True)

    def _setuintbe(self, uintbe: int, length: Optional[int] = None, _offset: None = None) -> None:
        """Set the bitstring to a big-endian unsigned int interpretation."""
        if length is None and hasattr(self, 'len') and self.len != 0:
            length = self.len
        if length is None or length == 0:
            raise CreationError("A non-zero length must be specified with a uintbe initialiser.")
        self._bitstore = _uintbe2bitstore(uintbe, length)

    def _readuintbe(self, start: int, length: int) -> int:
        """Read bits and interpret as a big-endian unsigned int."""
        if length % 8:
            raise InterpretError(f"Big-endian integers must be whole-byte. Length = {length} bits.")
        return self._readuint(start, length)

    def _getuintbe(self) -> int:
        """Return data as a big-endian two's complement unsigned int."""
        return self._readuintbe(0, self.len)

    def _setintbe(self, intbe: int, length: Optional[int] = None, _offset: None = None) -> None:
        """Set bitstring to a big-endian signed int interpretation."""
        if length is None and hasattr(self, 'len') and self.len != 0:
            length = self.len
        if length is None or length == 0:
            raise CreationError("A non-zero length must be specified with a intbe initialiser.")
        self._bitstore = _intbe2bitstore(intbe, length)

    def _readintbe(self, start: int, length: int) -> int:
        """Read bits and interpret as a big-endian signed int."""
        if length % 8:
            raise InterpretError(f"Big-endian integers must be whole-byte. Length = {length} bits.")
        return self._readint(start, length)

    def _getintbe(self) -> int:
        """Return data as a big-endian two's complement signed int."""
        return self._readintbe(0, self.len)

    def _setuintle(self, uintle: int, length: Optional[int] = None, _offset: None = None) -> None:
        if length is None and hasattr(self, 'len') and self.len != 0:
            length = self.len
        if length is None or length == 0:
            raise CreationError("A non-zero length must be specified with a uintle initialiser.")
        if _offset is not None:
            raise CreationError("An offset can't be specified with an integer initialiser.")
        self._bitstore = _uintle2bitstore(uintle, length)

    def _readuintle(self, start: int, length: int) -> int:
        """Read bits and interpret as a little-endian unsigned int."""
        if length % 8:
            raise InterpretError(f"Little-endian integers must be whole-byte. Length = {length} bits.")
        bs = BitStore(frombytes=self._bitstore.getslice(slice(start, start + length, None)).tobytes()[::-1])
        val = bitarray.util.ba2int(bs, signed=False)
        return val

    def _getuintle(self) -> int:
        return self._readuintle(0, self.len)

    def _setintle(self, intle: int, length: Optional[int] = None, _offset: None = None) -> None:
        if length is None and hasattr(self, 'len') and self.len != 0:
            length = self.len
        if length is None or length == 0:
            raise CreationError("A non-zero length must be specified with an intle initialiser.")
        if _offset is not None:
            raise CreationError("An offset can't be specified with an integer initialiser.")
        self._bitstore = _intle2bitstore(intle, length)

    def _readintle(self, start: int, length: int) -> int:
        """Read bits and interpret as a little-endian signed int."""
        if length % 8:
            raise InterpretError(f"Little-endian integers must be whole-byte. Length = {length} bits.")
        bs = BitStore(frombytes=self._bitstore.getslice(slice(start, start + length, None)).tobytes()[::-1])
        val = bitarray.util.ba2int(bs, signed=True)
        return val

    def _getintle(self) -> int:
        return self._readintle(0, self.len)

    def _readfloat(self, start: int, length: int, struct_dict: Dict[int, str]) -> float:
        """Read bits and interpret as a float."""
        try:
            fmt = struct_dict[length]
        except KeyError:
            raise InterpretError(f"Floats can only be 16, 32 or 64 bits long, not {length} bits")

        offset = start % 8
        if offset == 0:
            return struct.unpack(fmt, self._bitstore.getslice(slice(start, start + length, None)).tobytes())[0]
        else:
            return struct.unpack(fmt, self._readbytes(start, length))[0]

    def _readfloat143(self, start: int, length: int = 0) -> float:
        # length is ignored - it's only present to make the function signature consistent.
        u = self._readuint(start, length=8)
        return fp143_fmt.lut_int8_to_float[u]

    def _getfloat143(self) -> float:
        if len(self) != 8:
            raise InterpretError(f"A float8_143 must be 8 bits long, not {len(self)} bits.")
        return self._readfloat143(0)

    def _readfloat152(self, start: int, length: int = 0) -> float:
        # length is ignored - it's only present to make the function signature consistent.
        u = self._readuint(start, length=8)
        return fp152_fmt.lut_int8_to_float[u]

    def _getfloat152(self) -> float:
        if len(self) != 8:
            raise InterpretError(f"A float8_152 must be 8 bits long, not {len(self)} bits.")
        return self._readfloat152(start=0)

    def _setfloatbe(self, f: float, length: Optional[int] = None, _offset: None = None) -> None:
        if length is None and hasattr(self, 'len') and self.len != 0:
            length = self.len
        if length is None or length not in [16, 32, 64]:
            raise CreationError("A length of 16, 32, or 64 must be specified with a float initialiser.")
        self._bitstore = _float2bitstore(f, length)

    def _readfloatbe(self, start: int, length: int) -> float:
        """Read bits and interpret as a big-endian float."""
        return self._readfloat(start, length, {16: '>e', 32: '>f', 64: '>d'})

    def _getfloatbe(self) -> float:
        """Interpret the whole bitstring as a big-endian float."""
        return self._readfloatbe(0, self.len)

    def _setfloatle(self, f: float, length: Optional[int] = None, _offset: None = None) -> None:
        if length is None and hasattr(self, 'len') and self.len != 0:
            length = self.len
        if length is None or length not in [16, 32, 64]:
            raise CreationError("A length of 16, 32, or 64 must be specified with a float initialiser.")
        self._bitstore = _floatle2bitstore(f, length)

    def _readfloatle(self, start: int, length: int) -> float:
        """Read bits and interpret as a little-endian float."""
        return self._readfloat(start, length, {16: '<e', 32: '<f', 64: '<d'})

    def _getfloatle(self) -> float:
        """Interpret the whole bitstring as a little-endian float."""
        return self._readfloatle(0, self.len)

    def _getbfloatbe(self) -> float:
        return self._readbfloatbe(0, self.len)

    def _readbfloatbe(self, start: int, length: int) -> float:
        if length != 16:
            raise InterpretError(f"bfloats must be length 16, received a length of {length} bits.")
        two_bytes = self._slice(start, start + 16)
        zero_padded = two_bytes + Bits(16)
        return zero_padded._getfloatbe()

    def _setbfloatbe(self, f: Union[float, str], length: Optional[int] = None, _offset: None = None) -> None:
        if length is not None and length != 16:
            raise CreationError(f"bfloats must be length 16, received a length of {length} bits.")
        self._bitstore = _bfloat2bitstore(f)

    def _getbfloatle(self) -> float:
        return self._readbfloatle(0, self.len)

    def _readbfloatle(self, start: int, length: int) -> float:
        two_bytes = self._slice(start, start + 16)
        zero_padded = Bits(16) + two_bytes
        return zero_padded._getfloatle()

    def _setbfloatle(self, f: Union[float, str], length: Optional[int] = None, _offset: None = None) -> None:
        if length is not None and length != 16:
            raise CreationError(f"bfloats must be length 16, received a length of {length} bits.")
        self._bitstore = _bfloatle2bitstore(f)

    def _setue(self, i: int, _length: None = None, _offset: None = None) -> None:
        """Initialise bitstring with unsigned exponential-Golomb code for integer i.

        Raises CreationError if i < 0.

        """
        if _length is not None or _offset is not None:
            raise CreationError("Cannot specify a length of offset for exponential-Golomb codes.")
        self._bitstore = _ue2bitstore(i)

    def _readue(self, pos: int, _length: int = 0) -> Tuple[int, int]:
        """Return interpretation of next bits as unsigned exponential-Golomb code.

        Raises ReadError if the end of the bitstring is encountered while
        reading the code.

        """
        # _length is ignored - it's only present to make the function signature consistent.
        if _lsb0:
            raise ReadError("Exp-Golomb codes cannot be read in lsb0 mode.")
        oldpos = pos
        try:
            while not self[pos]:
                pos += 1
        except IndexError:
            raise ReadError("Read off end of bitstring trying to read code.")
        leadingzeros = pos - oldpos
        codenum = (1 << leadingzeros) - 1
        if leadingzeros > 0:
            if pos + leadingzeros + 1 > self.len:
                raise ReadError("Read off end of bitstring trying to read code.")
            codenum += self._readuint(pos + 1, leadingzeros)
            pos += leadingzeros + 1
        else:
            assert codenum == 0
            pos += 1
        return codenum, pos

    def _getue(self) -> int:
        """Return data as unsigned exponential-Golomb code.

        Raises InterpretError if bitstring is not a single exponential-Golomb code.

        """
        try:
            value, newpos = self._readue(0)
            if value is None or newpos != self.len:
                raise ReadError
        except ReadError:
            raise InterpretError("Bitstring is not a single exponential-Golomb code.")
        return value

    def _setse(self, i: int, _length: None = None, _offset: None = None) -> None:
        """Initialise bitstring with signed exponential-Golomb code for integer i."""
        if _length is not None or _offset is not None:
            raise CreationError("Cannot specify a length of offset for exponential-Golomb codes.")
        self._bitstore = _se2bitstore(i)

    def _getse(self) -> int:
        """Return data as signed exponential-Golomb code.

        Raises InterpretError if bitstring is not a single exponential-Golomb code.

        """
        try:
            value, newpos = self._readse(0)
            if value is None or newpos != self.len:
                raise ReadError
        except ReadError:
            raise InterpretError("Bitstring is not a single exponential-Golomb code.")
        return value

    def _readse(self, pos: int, _length: int = 0) -> Tuple[int, int]:
        """Return interpretation of next bits as a signed exponential-Golomb code.

        Advances position to after the read code.

        Raises ReadError if the end of the bitstring is encountered while
        reading the code.

        """
        codenum, pos = self._readue(pos)
        m = (codenum + 1) // 2
        if not codenum % 2:
            return -m, pos
        else:
            return m, pos

    def _setuie(self, i: int, _length: None = None, _offset: None = None) -> None:
        """Initialise bitstring with unsigned interleaved exponential-Golomb code for integer i.

        Raises CreationError if i < 0.

        """
        if _length is not None or _offset is not None:
            raise CreationError("Cannot specify a length of offset for exponential-Golomb codes.")
        self._bitstore = _uie2bitstore(i)

    def _readuie(self, pos: int, _length: int = 0) -> Tuple[int, int]:
        """Return interpretation of next bits as unsigned interleaved exponential-Golomb code.

        Raises ReadError if the end of the bitstring is encountered while
        reading the code.

        """
        # _length is ignored - it's only present to make the function signature consistent.
        if _lsb0:
            raise ReadError("Exp-Golomb codes cannot be read in lsb0 mode.")
        try:
            codenum: int = 1
            while not self[pos]:
                pos += 1
                codenum <<= 1
                codenum += self[pos]
                pos += 1
            pos += 1
        except IndexError:
            raise ReadError("Read off end of bitstring trying to read code.")
        codenum -= 1
        return codenum, pos

    def _getuie(self) -> int:
        """Return data as unsigned interleaved exponential-Golomb code.

        Raises InterpretError if bitstring is not a single exponential-Golomb code.

        """
        try:
            value, newpos = self._readuie(0)
            if value is None or newpos != self.len:
                raise ReadError
        except ReadError:
            raise InterpretError("Bitstring is not a single interleaved exponential-Golomb code.")
        return value

    def _setsie(self, i: int, _length: None = None, _offset: None = None) -> None:
        """Initialise bitstring with signed interleaved exponential-Golomb code for integer i."""
        if _length is not None or _offset is not None:
            raise CreationError("Cannot specify a length of offset for exponential-Golomb codes.")
        self._bitstore = _sie2bitstore(i)

    def _getsie(self) -> int:
        """Return data as signed interleaved exponential-Golomb code.

        Raises InterpretError if bitstring is not a single exponential-Golomb code.

        """
        try:
            value, newpos = self._readsie(0)
            if value is None or newpos != self.len:
                raise ReadError
        except ReadError:
            raise InterpretError("Bitstring is not a single interleaved exponential-Golomb code.")
        return value

    def _readsie(self, pos: int, _length: int = 0) -> Tuple[int, int]:
        """Return interpretation of next bits as a signed interleaved exponential-Golomb code.

        Advances position to after the read code.

        Raises ReadError if the end of the bitstring is encountered while
        reading the code.

        """
        codenum, pos = self._readuie(pos)
        if not codenum:
            return 0, pos
        try:
            if self[pos]:
                return -codenum, pos + 1
            else:
                return codenum, pos + 1
        except IndexError:
            raise ReadError("Read off end of bitstring trying to read code.")

    def _setbool(self, value: Union[bool, str], length: Optional[int] = None, _offset: None = None) -> None:
        # We deliberately don't want to have implicit conversions to bool here.
        # If we did then it would be difficult to deal with the 'False' string.
        if length is not None and length != 1:
            raise CreationError(f"bools must be length 1, received a length of {length} bits.")
        if value in (1, 'True'):
            self._bitstore = BitStore('1')
        elif value in (0, 'False'):
            self._bitstore = BitStore('0')
        else:
            raise CreationError(f"Cannot initialise boolean with {value}.")

    def _getbool(self) -> bool:
        if self.length != 1:
            raise InterpretError(f"For a bool interpretation a bitstring must be 1 bit long, not {self.length} bits.")
        return self[0]

    def _readbool(self, start: int, length: int = 0) -> int:
        # length is ignored - it's only present to make the function signature consistent.
        return self[start]

    def _readpad(self, _pos, _length) -> None:
        return None

    def _setbin_safe(self, binstring: str, length: None = None, _offset: None = None) -> None:
        """Reset the bitstring to the value given in binstring."""
        self._bitstore = _bin2bitstore(binstring)

    def _setbin_unsafe(self, binstring: str, length: None = None, _offset: None = None) -> None:
        """Same as _setbin_safe, but input isn't sanity checked. binstring mustn't start with '0b'."""
        self._bitstore = _bin2bitstore_unsafe(binstring)

    def _readbin(self, start: int, length: int) -> str:
        """Read bits and interpret as a binary string."""
        if length == 0:
            return ''
        return self._bitstore.getslice(slice(start, start + length, None)).to01()

    def _getbin(self) -> str:
        """Return interpretation as a binary string."""
        return self._readbin(0, self.len)

    def _setoct(self, octstring: str, length: None = None, _offset: None = None) -> None:
        """Reset the bitstring to have the value given in octstring."""
        self._bitstore = _oct2bitstore(octstring)

    def _readoct(self, start: int, length: int) -> str:
        """Read bits and interpret as an octal string."""
        if length % 3:
            raise InterpretError("Cannot convert to octal unambiguously - not multiple of 3 bits long.")
        s = bitarray.util.ba2base(8, self._bitstore.getslice(slice(start, start + length, None)))
        return s

    def _getoct(self) -> str:
        """Return interpretation as an octal string."""
        if self.len % 3:
            raise InterpretError("Cannot convert to octal unambiguously - not multiple of 3 bits long.")
        ba = self._bitstore.copy() if self._bitstore.modified else self._bitstore
        return bitarray.util.ba2base(8, ba)

    def _sethex(self, hexstring: str, length: None = None, _offset: None = None) -> None:
        """Reset the bitstring to have the value given in hexstring."""
        self._bitstore = _hex2bitstore(hexstring)

    def _readhex(self, start: int, length: int) -> str:
        """Read bits and interpret as a hex string."""
        if length % 4:
            raise InterpretError("Cannot convert to hex unambiguously - not a multiple of 4 bits long.")
        return bitarray.util.ba2hex(self._bitstore.getslice(slice(start, start + length, None)))

    def _gethex(self) -> str:
        """Return the hexadecimal representation as a string.

        Raises an InterpretError if the bitstring's length is not a multiple of 4.

        """
        if self.len % 4:
            raise InterpretError("Cannot convert to hex unambiguously - not a multiple of 4 bits long.")
        ba = self._bitstore.copy() if self._bitstore.modified else self._bitstore
        return bitarray.util.ba2hex(ba)

    def _getlength(self) -> int:
        """Return the length of the bitstring in bits."""
        return len(self._bitstore)

    def _copy(self: TBits) -> TBits:
        """Create and return a new copy of the Bits (always in memory)."""
        # Note that __copy__ may choose to return self if it's immutable. This method always makes a copy.
        s_copy = self.__class__()
        s_copy._bitstore = self._bitstore.copy()
        return s_copy

    def _slice(self: TBits, start: int, end: int) -> TBits:
        """Used internally to get a slice, without error checking."""
        bs = self.__class__()
        bs._bitstore = self._bitstore.getslice(slice(start, end, None))
        return bs

    def _absolute_slice(self: TBits, start: int, end: int) -> TBits:
        """Used internally to get a slice, without error checking.
        Uses MSB0 bit numbering even if LSB0 is set."""
        if end == start:
            return self.__class__()
        assert start < end, f"start={start}, end={end}"
        bs = self.__class__()
        bs._bitstore = self._bitstore.getslice_msb0(slice(start, end, None))
        return bs

    def _readtoken(self, name: str, pos: int, length: Optional[int]) -> Tuple[Union[float, int, str, None, Bits], int]:
        """Reads a token from the bitstring and returns the result."""
        if length is not None and length > self.length - pos:
            raise ReadError("Reading off the end of the data. "
                            f"Tried to read {length} bits when only {self.length - pos} available.")
        try:
            val = self._name_to_read[name](self, pos, length)
            if isinstance(val, tuple):
                return val
            else:
                assert length is not None
                return val, pos + length
        except KeyError:
            raise ValueError(f"Can't parse token {name}:{length}")

    def _addright(self, bs: Bits) -> None:
        """Add a bitstring to the RHS of the current bitstring."""
        self._bitstore += bs._bitstore

    def _addleft(self, bs: Bits) -> None:
        """Prepend a bitstring to the current bitstring."""
        if bs._bitstore.immutable:
            self._bitstore = bs._bitstore.copy() + self._bitstore
        else:
            self._bitstore = bs._bitstore + self._bitstore

    def _truncateleft(self: TBits, bits: int) -> TBits:
        """Truncate bits from the start of the bitstring. Return the truncated bits."""
        assert 0 <= bits <= self.len
        if not bits:
            return self.__class__()
        truncated_bits = self._absolute_slice(0, bits)
        if bits == self.len:
            self._clear()
            return truncated_bits
        self._bitstore = self._bitstore.getslice_msb0(slice(bits, None, None))
        return truncated_bits

    def _truncateright(self: TBits, bits: int) -> TBits:
        """Truncate bits from the end of the bitstring. Return the truncated bits."""
        assert 0 <= bits <= self.len
        if bits == 0:
            return self.__class__()
        truncated_bits = self._absolute_slice(self.length - bits, self.length)
        if bits == self.len:
            self._clear()
            return truncated_bits
        self._bitstore = self._bitstore.getslice_msb0(slice(None, -bits, None))
        return truncated_bits

    def _insert(self, bs: Bits, pos: int) -> None:
        """Insert bs at pos."""
        assert 0 <= pos <= self.len
        self._bitstore[pos: pos] = bs._bitstore
        return

    def _overwrite(self, bs: Bits, pos: int) -> None:
        """Overwrite with bs at pos."""
        assert 0 <= pos <= self.len
        if bs is self:
            # Just overwriting with self, so do nothing.
            assert pos == 0
            return
        self._bitstore[pos: pos + bs.len] = bs._bitstore

    def _delete(self, bits: int, pos: int) -> None:
        """Delete bits at pos."""
        assert 0 <= pos <= self.len
        assert pos + bits <= self.len, f"pos={pos}, bits={bits}, len={self.len}"
        del self._bitstore[pos: pos + bits]
        return

    def _reversebytes(self, start: int, end: int) -> None:
        """Reverse bytes in-place."""
        assert (end - start) % 8 == 0
        self._bitstore[start:end] = BitStore(frombytes=self._bitstore.getslice(slice(start, end, None)).tobytes()[::-1])

    def _invert(self, pos: int) -> None:
        """Flip bit at pos 1<->0."""
        assert 0 <= pos < self.len
        self._bitstore.invert(pos)

    def _invert_all(self) -> None:
        """Invert every bit."""
        self._bitstore.invert()

    def _ilshift(self: TBits, n: int) -> TBits:
        """Shift bits by n to the left in place. Return self."""
        assert 0 < n <= self.len
        self._addright(Bits(n))
        self._truncateleft(n)
        return self

    def _irshift(self: TBits, n: int) -> TBits:
        """Shift bits by n to the right in place. Return self."""
        assert 0 < n <= self.len
        self._addleft(Bits(n))
        self._truncateright(n)
        return self

    def _imul(self: TBits, n: int) -> TBits:
        """Concatenate n copies of self in place. Return self."""
        assert n >= 0
        if not n:
            self._clear()
            return self
        m: int = 1
        old_len: int = self.len
        while m * 2 < n:
            self._addright(self)
            m *= 2
        self._addright(self[0:(n - m) * old_len])
        return self

    def _readbits(self: TBits, start: int, length: int) -> TBits:
        """Read some bits from the bitstring and return newly constructed bitstring."""
        return self._slice(start, start + length)

    def _validate_slice(self, start: Optional[int], end: Optional[int]) -> Tuple[int, int]:
        """Validate start and end and return them as positive bit positions."""
        if start is None:
            start = 0
        elif start < 0:
            start += self._getlength()
        if end is None:
            end = self._getlength()
        elif end < 0:
            end += self._getlength()
        if not 0 <= end <= self._getlength():
            raise ValueError("end is not a valid position in the bitstring.")
        if not 0 <= start <= self._getlength():
            raise ValueError("start is not a valid position in the bitstring.")
        if end < start:
            raise ValueError("end must not be less than start.")
        return start, end

    def unpack(self, fmt: Union[str, List[Union[str, int]]], **kwargs) -> List[Union[int, float, str, Bits, bool, bytes, None]]:
        """Interpret the whole bitstring using fmt and return list.

        fmt -- A single string or a list of strings with comma separated tokens
               describing how to interpret the bits in the bitstring. Items
               can also be integers, for reading new bitstring of the given length.
        kwargs -- A dictionary or keyword-value pairs - the keywords used in the
                  format string will be replaced with their given value.

        Raises ValueError if the format is not understood. If not enough bits
        are available then all bits to the end of the bitstring will be used.

        See the docstring for 'read' for token examples.

        """
        return self._readlist(fmt, 0, **kwargs)[0]

    def _readlist(self, fmt: Union[str, List[Union[str, int]]], pos: int, **kwargs: int) \
            -> Tuple[List[Union[int, float, str, Bits, bool, bytes, None]], int]:
        tokens: List[Tuple[str, Optional[Union[str, int]], Optional[str]]] = []
        if isinstance(fmt, str):
            fmt = [fmt]
        keys: Tuple[str, ...] = tuple(sorted(kwargs.keys()))

        def convert_length_strings(length_: Optional[Union[str, int]]) -> Optional[int]:
            int_length: Optional[int] = None
            if isinstance(length_, str):
                if length_ in kwargs:
                    int_length = kwargs[length_]
                    if name == 'bytes':
                        int_length *= 8
            else:
                int_length = length_
            return int_length

        has_stretchy_token = False
        for f_item in fmt:
            # Replace integers with 'bits' tokens
            if isinstance(f_item, numbers.Integral):
                tokens.append(('bits', f_item, None))
            else:
                stretchy, tkns = tokenparser(f_item, keys)
                if stretchy:
                    if has_stretchy_token:
                        raise Error("It's not possible to have more than one 'filler' token.")
                    has_stretchy_token = True
                tokens.extend(tkns)
        if not has_stretchy_token:
            lst = []
            for name, length, _ in tokens:
                length = convert_length_strings(length)
                if name in kwargs and length is None:
                    # Using default 'bits' - the name is really the length.
                    value, pos = self._readtoken('bits', pos, kwargs[name])
                    lst.append(value)
                    continue
                value, pos = self._readtoken(name, pos, length)
                if value is not None:  # Don't append pad tokens
                    lst.append(value)
            return lst, pos
        stretchy_token: Optional[tuple] = None
        bits_after_stretchy_token = 0
        for token in tokens:
            name, length, _ = token
            length = convert_length_strings(length)
            if stretchy_token:
                if name in ('se', 'ue', 'sie', 'uie'):
                    raise Error("It's not possible to parse a variable length token after a 'filler' token.")
                else:
                    if length is None:
                        raise Error("It's not possible to have more than one 'filler' token.")
                    bits_after_stretchy_token += length
            if length is None and name not in ('se', 'ue', 'sie', 'uie'):
                assert not stretchy_token
                stretchy_token = token
        bits_left = self.len - pos
        return_values = []
        for token in tokens:
            name, length, _ = token
            if token is stretchy_token:
                # Set length to the remaining bits
                length = max(bits_left - bits_after_stretchy_token, 0)
            length = convert_length_strings(length)
            value, newpos = self._readtoken(name, pos, length)
            bits_left -= newpos - pos
            pos = newpos
            if value is not None:
                return_values.append(value)
        return return_values, pos

    def find(self, bs: BitsType, start: Optional[int] = None, end: Optional[int] = None,
             bytealigned: Optional[bool] = None) -> Union[Tuple[int], Tuple[()]]:
        """Find first occurrence of substring bs.

        Returns a single item tuple with the bit position if found, or an
        empty tuple if not found. The bit position (pos property) will
        also be set to the start of the substring if it is found.

        bs -- The bitstring to find.
        start -- The bit position to start the search. Defaults to 0.
        end -- The bit position one past the last bit to search.
               Defaults to len(self).
        bytealigned -- If True the bitstring will only be
                       found on byte boundaries.

        Raises ValueError if bs is empty, if start < 0, if end > len(self) or
        if end < start.

        >>> BitArray('0xc3e').find('0b1111')
        (6,)

        """
        bs = Bits._create_from_bitstype(bs)
        if len(bs) == 0:
            raise ValueError("Cannot find an empty bitstring.")
        start, end = self._validate_slice(start, end)
        ba = _bytealigned if bytealigned is None else bytealigned
        p = self._find(bs, start, end, ba)
        return p

    def _find_lsb0(self, bs: Bits, start: int, end: int, bytealigned: bool) -> Union[Tuple[int], Tuple[()]]:
        # A forward find in lsb0 is very like a reverse find in msb0.
        assert start <= end
        assert _lsb0

        new_slice = _offset_slice_indices_lsb0(slice(start, end, None), len(self), 0)
        msb0_start, msb0_end = self._validate_slice(new_slice.start, new_slice.stop)
        p = self._rfind_msb0(bs, msb0_start, msb0_end, bytealigned)

        if p:
            return (self.length - p[0] - bs.length,)
        else:
            return ()

    def _find_msb0(self, bs: Bits, start: int, end: int, bytealigned: bool) -> Union[Tuple[int], Tuple[()]]:
        """Find first occurrence of a binary string."""
        while True:
            p = self._bitstore.find(bs._bitstore, start, end)
            if p == -1:
                return ()
            if not bytealigned or (p % 8) == 0:
                return (p,)
            # Advance to just beyond the non-byte-aligned match and try again...
            start = p + 1

    def findall(self, bs: BitsType, start: Optional[int] = None, end: Optional[int] = None, count: Optional[int] = None,
                bytealigned: Optional[bool] = None) -> Iterable[int]:
        """Find all occurrences of bs. Return generator of bit positions.

        bs -- The bitstring to find.
        start -- The bit position to start the search. Defaults to 0.
        end -- The bit position one past the last bit to search.
               Defaults to len(self).
        count -- The maximum number of occurrences to find.
        bytealigned -- If True the bitstring will only be found on
                       byte boundaries.

        Raises ValueError if bs is empty, if start < 0, if end > len(self) or
        if end < start.

        Note that all occurrences of bs are found, even if they overlap.

        """
        if count is not None and count < 0:
            raise ValueError("In findall, count must be >= 0.")
        bs = Bits._create_from_bitstype(bs)
        start, end = self._validate_slice(start, end)
        ba = _bytealigned if bytealigned is None else bytealigned
        return self._findall(bs, start, end, count, ba)

    def _findall_msb0(self, bs: Bits, start: int, end: int, count: Optional[int],
                      bytealigned: bool) -> Iterable[int]:
        c = 0
        for i in self._bitstore.getslice_msb0(slice(start, end, None)).itersearch(bs._bitstore):
            if count is not None and c >= count:
                return
            if bytealigned:
                if (start + i) % 8 == 0:
                    c += 1
                    yield start + i
            else:
                c += 1
                yield start + i
        return

    def _findall_lsb0(self, bs: Bits, start: int, end: int, count: Optional[int],
                      bytealigned: bool) -> Iterable[int]:
        assert start <= end
        assert _lsb0

        new_slice = _offset_slice_indices_lsb0(slice(start, end, None), len(self), 0)
        msb0_start, msb0_end = self._validate_slice(new_slice.start, new_slice.stop)

        # Search chunks starting near the end and then moving back.
        c = 0
        increment = max(8192, bs.len * 80)
        buffersize = min(increment + bs.len, msb0_end - msb0_start)
        pos = max(msb0_start, msb0_end - buffersize)
        while True:
            found = list(self._findall_msb0(bs, start=pos, end=pos + buffersize, count=None, bytealigned=False))
            if not found:
                if pos == msb0_start:
                    return
                pos = max(msb0_start, pos - increment)
                continue
            while found:
                if count is not None and c >= count:
                    return
                c += 1
                lsb0_pos = self.len - found.pop() - bs.len
                if not bytealigned or lsb0_pos % 8 == 0:
                    yield lsb0_pos

            pos = max(msb0_start, pos - increment)
            if pos == msb0_start:
                return

    def rfind(self, bs: BitsType, start: Optional[int] = None, end: Optional[int] = None,
              bytealigned: Optional[bool] = None) -> Union[Tuple[int], Tuple[()]]:
        """Find final occurrence of substring bs.

        Returns a single item tuple with the bit position if found, or an
        empty tuple if not found. The bit position (pos property) will
        also be set to the start of the substring if it is found.

        bs -- The bitstring to find.
        start -- The bit position to end the reverse search. Defaults to 0.
        end -- The bit position one past the first bit to reverse search.
               Defaults to len(self).
        bytealigned -- If True the bitstring will only be found on byte
                       boundaries.

        Raises ValueError if bs is empty, if start < 0, if end > len(self) or
        if end < start.

        """
        bs = Bits._create_from_bitstype(bs)
        start, end = self._validate_slice(start, end)
        ba = _bytealigned if bytealigned is None else bytealigned
        if not bs.len:
            raise ValueError("Cannot find an empty bitstring.")
        p = self._rfind(bs, start, end, ba)
        return p

    def _rfind_msb0(self, bs: Bits, start: int, end: int, bytealigned: bool) -> Union[Tuple[int], Tuple[()]]:
        """Find final occurrence of a binary string."""
        increment = max(4096, len(bs) * 64)
        buffersize = increment + len(bs)
        p = end
        while p > start:
            start_pos = max(start, p - buffersize)
            ps = list(self._findall_msb0(bs, start_pos, p, count=None, bytealigned=False))
            if ps:
                while ps:
                    if not bytealigned or (ps[-1] % 8 == 0):
                        return (ps[-1],)
                    ps.pop()
            p -= increment
        return ()

    def _rfind_lsb0(self, bs: Bits, start: int, end: int, bytealigned: bool) -> Union[Tuple[int], Tuple[()]]:
        # A reverse find in lsb0 is very like a forward find in msb0.
        assert start <= end
        assert _lsb0
        new_slice = _offset_slice_indices_lsb0(slice(start, end, None), len(self), 0)
        msb0_start, msb0_end = self._validate_slice(new_slice.start, new_slice.stop)

        p = self._find_msb0(bs, msb0_start, msb0_end, bytealigned)
        if p:
            return (self.len - p[0] - bs.length,)
        else:
            return ()

    def cut(self, bits: int, start: Optional[int] = None, end: Optional[int] = None,
            count: Optional[int] = None) -> Iterator[Bits]:
        """Return bitstring generator by cutting into bits sized chunks.

        bits -- The size in bits of the bitstring chunks to generate.
        start -- The bit position to start the first cut. Defaults to 0.
        end -- The bit position one past the last bit to use in the cut.
               Defaults to len(self).
        count -- If specified then at most count items are generated.
                 Default is to cut as many times as possible.

        """
        start_, end_ = self._validate_slice(start, end)
        if count is not None and count < 0:
            raise ValueError("Cannot cut - count must be >= 0.")
        if bits <= 0:
            raise ValueError("Cannot cut - bits must be >= 0.")
        c = 0
        while count is None or c < count:
            c += 1
            nextchunk = self._slice(start_, min(start_ + bits, end_))
            if nextchunk.len == 0:
                return
            yield nextchunk
            if nextchunk._getlength() != bits:
                return
            start_ += bits
        return

    def split(self, delimiter: BitsType, start: Optional[int] = None, end: Optional[int] = None,
              count: Optional[int] = None, bytealigned: Optional[bool] = None) -> Iterable[Bits]:
        """Return bitstring generator by splitting using a delimiter.

        The first item returned is the initial bitstring before the delimiter,
        which may be an empty bitstring.

        delimiter -- The bitstring used as the divider.
        start -- The bit position to start the split. Defaults to 0.
        end -- The bit position one past the last bit to use in the split.
               Defaults to len(self).
        count -- If specified then at most count items are generated.
                 Default is to split as many times as possible.
        bytealigned -- If True splits will only occur on byte boundaries.

        Raises ValueError if the delimiter is empty.

        """
        delimiter = Bits._create_from_bitstype(delimiter)
        if len(delimiter) == 0:
            raise ValueError("split delimiter cannot be empty.")
        start, end = self._validate_slice(start, end)
        bytealigned_: bool = _bytealigned if bytealigned is None else bytealigned
        if count is not None and count < 0:
            raise ValueError("Cannot split - count must be >= 0.")
        if count == 0:
            return
        f = functools.partial(self._find_msb0, bs=delimiter, bytealigned=bytealigned_)
        found = f(start=start, end=end)
        if not found:
            # Initial bits are the whole bitstring being searched
            yield self._slice(start, end)
            return
        # yield the bytes before the first occurrence of the delimiter, even if empty
        yield self._slice(start, found[0])
        startpos = pos = found[0]
        c = 1
        while count is None or c < count:
            pos += delimiter.len
            found = f(start=pos, end=end)
            if not found:
                # No more occurrences, so return the rest of the bitstring
                yield self._slice(startpos, end)
                return
            c += 1
            yield self._slice(startpos, found[0])
            startpos = pos = found[0]
        # Have generated count bitstrings, so time to quit.
        return

    def join(self: TBits, sequence: Iterable[Any]) -> TBits:
        """Return concatenation of bitstrings joined by self.

        sequence -- A sequence of bitstrings.

        """
        s = self.__class__()
        if len(self) == 0:
            # Optimised version that doesn't need to add self between every item
            for item in sequence:
                s._addright(Bits._create_from_bitstype(item))
        else:
            i = iter(sequence)
            try:
                s._addright(Bits._create_from_bitstype(next(i)))
                while True:
                    n = next(i)
                    s._addright(self)
                    s._addright(Bits._create_from_bitstype(n))
            except StopIteration:
                pass
        return s

    def tobytes(self) -> bytes:
        """Return the bitstring as bytes, padding with zero bits if needed.

        Up to seven zero bits will be added at the end to byte align.

        """
        return self._bitstore.tobytes()

    def tobitarray(self) -> bitarray.bitarray:
        """Convert the bitstring to a bitarray object."""
        if self._bitstore.modified:
            # Removes the offset and truncates to length
            return bitarray.bitarray(self._bitstore.copy())
        else:
            return bitarray.bitarray(self._bitstore)

    def tofile(self, f: BinaryIO) -> None:
        """Write the bitstring to a file object, padding with zero bits if needed.

        Up to seven zero bits will be added at the end to byte align.

        """
        # If the bitstring is file based then we don't want to read it all in to memory first.
        chunk_size = 8 * 100 * 1024 * 1024  # 100 MiB
        for chunk in self.cut(chunk_size):
            f.write(chunk.tobytes())

    def startswith(self, prefix: BitsType, start: Optional[int] = None, end: Optional[int] = None) -> bool:
        """Return whether the current bitstring starts with prefix.

        prefix -- The bitstring to search for.
        start -- The bit position to start from. Defaults to 0.
        end -- The bit position to end at. Defaults to len(self).

        """
        prefix = self._create_from_bitstype(prefix)
        start, end = self._validate_slice(start, end)
        if end < start + prefix._getlength():
            return False
        end = start + prefix._getlength()
        return self._slice(start, end) == prefix

    def endswith(self, suffix: BitsType, start: Optional[int] = None, end: Optional[int] = None) -> bool:
        """Return whether the current bitstring ends with suffix.

        suffix -- The bitstring to search for.
        start -- The bit position to start from. Defaults to 0.
        end -- The bit position to end at. Defaults to len(self).

        """
        suffix = self._create_from_bitstype(suffix)
        start, end = self._validate_slice(start, end)
        if start + suffix.len > end:
            return False
        start = end - suffix._getlength()
        return self._slice(start, end) == suffix

    def all(self, value: Any, pos: Optional[Iterable[int]] = None) -> bool:
        """Return True if one or many bits are all set to bool(value).

        value -- If value is True then checks for bits set to 1, otherwise
                 checks for bits set to 0.
        pos -- An iterable of bit positions. Negative numbers are treated in
               the same way as slice indices. Defaults to the whole bitstring.

        """
        value = bool(value)
        length = self.len
        if pos is None:
            if value is True:
                return self._bitstore.all_set()
            else:
                return not self._bitstore.any_set()
        for p in pos:
            if p < 0:
                p += length
            if not 0 <= p < length:
                raise IndexError(f"Bit position {p} out of range.")
            if not bool(self._bitstore.getindex(p)) is value:
                return False
        return True

    def any(self, value: Any, pos: Optional[Iterable[int]] = None) -> bool:
        """Return True if any of one or many bits are set to bool(value).

        value -- If value is True then checks for bits set to 1, otherwise
                 checks for bits set to 0.
        pos -- An iterable of bit positions. Negative numbers are treated in
               the same way as slice indices. Defaults to the whole bitstring.

        """
        value = bool(value)
        length = self.len
        if pos is None:
            if value is True:
                return self._bitstore.any_set()
            else:
                return not self._bitstore.all_set()
        for p in pos:
            if p < 0:
                p += length
            if not 0 <= p < length:
                raise IndexError(f"Bit position {p} out of range.")
            if bool(self._bitstore.getindex(p)) is value:
                return True
        return False

    def count(self, value: Any) -> int:
        """Return count of total number of either zero or one bits.

        value -- If bool(value) is True then bits set to 1 are counted, otherwise bits set
                 to 0 are counted.

        >>> Bits('0xef').count(1)
        7

        """
        # count the number of 1s (from which it's easy to work out the 0s).
        count = self._bitstore.count(1)
        return count if value else self.len - count

    @staticmethod
    def _chars_in_pp_token(fmt: str) -> Tuple[str, Optional[int]]:
        """
        bin8 -> 'bin', 8
        hex12 -> 'hex', 3
        o9 -> 'oct', 3
        b -> 'bin', None
        """
        bpc_dict = {'bin': 1, 'oct': 3, 'hex': 4, 'bytes': 8}  # bits represented by each printed character
        short_token: Pattern[str] = re.compile(r'(?P<name>bytes|bin|oct|hex|b|o|h):?(?P<len>\d+)$')

        m1 = short_token.match(fmt)
        if m1:
            length = int(m1.group('len'))
            name = m1.group('name')
        else:
            length = None
            name = fmt
        aliases = {'hex': 'hex', 'oct': 'oct', 'bin': 'bin', 'bytes': 'bytes', 'b': 'bin', 'o': 'oct', 'h': 'hex'}
        try:
            name = aliases[name]
        except KeyError:
            pass  # Should be dealt with in the next check
        if name not in bpc_dict.keys():
            raise ValueError(f"Pretty print formats only support {'/'.join(bpc_dict.keys())}. Received '{fmt}'.")
        bpc = bpc_dict[name]
        if length is None:
            return name, None
        if length % bpc != 0:
            raise ValueError(f"Bits per group must be a multiple of {bpc} for '{fmt}' format.")
        return name, length

    @staticmethod
    def _format_bits(bits: Bits, chars_per_group: int, bits_per_group: int, sep: str, fmt: str, getter_fn=None) -> str:
        if fmt in ['bin', 'oct', 'hex', 'bytes']:
            raw = {'bin': bits._getbin,
                   'oct': bits._getoct,
                   'hex': bits._gethex,
                   'bytes': bits._getbytes_printable}[fmt]()
            if chars_per_group == 0:
                return raw
            formatted = sep.join(raw[i: i + chars_per_group] for i in range(0, len(raw), chars_per_group))
            return formatted

        else:
            if fmt == 'bits':
                formatted = sep.join(str(getter_fn(b, 0)) for b in bits.cut(bits_per_group))
                return formatted
            else:
                values = []
                for i in range(0, len(bits), bits_per_group):
                    b = bits[i: i + bits_per_group]
                    values.append(f"{getter_fn(b, 0): >{chars_per_group}}")
                formatted = sep.join(values)
                return formatted

    @staticmethod
    def _chars_per_group(bits_per_group: int, fmt: Optional[str]):
        if fmt is None:
            return 0
        bpc = {'bin': 1, 'oct': 3, 'hex': 4, 'bytes': 8}  # bits represented by each printed character
        try:
            return bits_per_group // bpc[fmt]
        except KeyError:
            # Work out how many chars are needed for each format given the number of bits
            if fmt in ['uint', 'uintne', 'uintbe', 'uintle']:
                # How many chars is largest uint?
                chars_per_value = len(str((1 << bits_per_group) - 1))
            elif fmt in ['int', 'intne', 'intbe', 'intle']:
                # Use largest negative int so we get the '-' sign
                chars_per_value = len(str((-1 << (bits_per_group - 1))))
            elif fmt in ['bfloat', 'bfloatne', 'bfloatbe', 'bfloatle']:
                chars_per_value = 23  # Empirical value
            elif fmt in ['float', 'floatne', 'floatbe', 'floatle']:
                if bits_per_group in [16, 32]:
                    chars_per_value = 23  # Empirical value
                elif bits_per_group == 64:
                    chars_per_value = 24  # Empirical value
            elif fmt == 'float8_143':
                chars_per_value = 13  # Empirical value
            elif fmt == 'float8_152':
                chars_per_value = 19  # Empirical value
            elif fmt == 'bool':
                chars_per_value = 1   # '0' or '1'
            elif fmt == 'bits':
                temp = BitArray(bits_per_group)
                chars_per_value = len(str(temp))
            else:
                assert False, f"Unsupported format string {fmt}."
                raise ValueError(f"Unsupported format string {fmt}.")
            return chars_per_value

    def _pp(self, name1: str, name2: Optional[str], bits_per_group: int, width: int, sep: str, format_sep: str,
            show_offset: bool, stream: TextIO, lsb0: bool, offset_factor: int, getter_fn=None, getter_fn2=None) -> None:
        """Internal pretty print method."""

        bpc = {'bin': 1, 'oct': 3, 'hex': 4, 'bytes': 8}  # bits represented by each printed character

        offset_width = 0
        offset_sep = ' :' if lsb0 else ': '
        if show_offset:
            # This could be 1 too large in some circumstances. Slightly recurrent logic needed to fix it...
            offset_width = len(str(len(self))) + len(offset_sep)
        if bits_per_group > 0:
            group_chars1 = Bits._chars_per_group(bits_per_group, name1)
            group_chars2 = Bits._chars_per_group(bits_per_group, name2)
            # The number of characters that get added when we add an extra group (after the first one)
            total_group_chars = group_chars1 + group_chars2 + len(sep) + len(sep) * bool(group_chars2)
            width_excluding_offset_and_final_group = width - offset_width - group_chars1 - group_chars2 - len(
                format_sep) * bool(group_chars2)
            width_excluding_offset_and_final_group = max(width_excluding_offset_and_final_group, 0)
            groups_per_line = 1 + width_excluding_offset_and_final_group // total_group_chars
            max_bits_per_line = groups_per_line * bits_per_group  # Number of bits represented on each line
        else:
            assert bits_per_group == 0  # Don't divide into groups
            group_chars1 = group_chars2 = 0
            width_available = width - offset_width - len(format_sep) * (name2 is not None)
            width_available = max(width_available, 1)
            if name2 is None:
                max_bits_per_line = width_available * bpc[name1]
            else:
                chars_per_24_bits = 24 // bpc[name1] + 24 // bpc[name2]
                max_bits_per_line = 24 * (width_available // chars_per_24_bits)
                if max_bits_per_line == 0:
                    max_bits_per_line = 24  # We can't fit into the width asked for. Show something small.
        assert max_bits_per_line > 0

        bitpos = 0
        first_fb_width = second_fb_width = None
        for bits in self.cut(max_bits_per_line):
            offset = bitpos // offset_factor
            if _lsb0:
                offset_str = f'{offset_sep}{offset: >{offset_width - len(offset_sep)}}' if show_offset else ''
            else:
                offset_str = f'{offset: >{offset_width - len(offset_sep)}}{offset_sep}' if show_offset else ''
            fb = Bits._format_bits(bits, group_chars1, bits_per_group, sep, name1, getter_fn)
            if first_fb_width is None:
                first_fb_width = len(fb)
            if len(fb) < first_fb_width:  # Pad final line with spaces to align it
                if _lsb0:
                    fb = ' ' * (first_fb_width - len(fb)) + fb
                else:
                    fb += ' ' * (first_fb_width - len(fb))
            fb2 = '' if name2 is None else format_sep + Bits._format_bits(bits, group_chars2, bits_per_group, sep, name2, getter_fn2)
            if second_fb_width is None:
                second_fb_width = len(fb2)
            if len(fb2) < second_fb_width:
                if _lsb0:
                    fb2 = ' ' * (second_fb_width - len(fb2)) + fb2
                else:
                    fb2 += ' ' * (second_fb_width - len(fb2))
            if _lsb0 is True:
                line_fmt = fb + fb2 + offset_str + '\n'
            else:
                line_fmt = offset_str + fb + fb2 + '\n'
            stream.write(line_fmt)
            bitpos += len(bits)
        return

    def pp(self, fmt: Optional[str] = None, width: int = 120, sep: str = ' ',
           show_offset: bool = True, stream: TextIO = sys.stdout) -> None:
        """Pretty print the bitstring's value.

        fmt -- Printed data format. One or two of 'bin', 'oct', 'hex' or 'bytes'.
              The number of bits represented in each printed group defaults to 8 for hex and bin,
              12 for oct and 32 for bytes. This can be overridden with an explicit length, e.g. 'hex:64'.
              Use a length of 0 to not split into groups, e.g. `bin:0`.
        width -- Max width of printed lines. Defaults to 120. A single group will always be printed
                 per line even if it exceeds the max width.
        sep -- A separator string to insert between groups. Defaults to a single space.
        show_offset -- If True (the default) shows the bit offset in the first column of each line.
        stream -- A TextIO object with a write() method. Defaults to sys.stdout.

        >>> s.pp('hex16')
        >>> s.pp('b, h', sep='_', show_offset=False)

        """
        if fmt is None:
            fmt = 'bin' if len(self) % 4 != 0 else 'bin, hex'

        bpc = {'bin': 1, 'oct': 3, 'hex': 4, 'bytes': 8}  # bits represented by each printed character

        formats = [f.strip() for f in fmt.split(',')]
        if len(formats) == 1:
            fmt1, fmt2 = formats[0], None
        elif len(formats) == 2:
            fmt1, fmt2 = formats[0], formats[1]
        else:
            raise ValueError(f"Either 1 or 2 comma separated formats must be specified, not {len(formats)}."
                             " Format string was {fmt}.")

        name1, length1 = Bits._chars_in_pp_token(fmt1)
        if fmt2 is not None:
            name2, length2 = Bits._chars_in_pp_token(fmt2)

        if fmt2 is not None and length2 is not None and length1 is not None:
            # Both lengths defined so must be equal
            if length1 != length2:
                raise ValueError(f"Differing bit lengths of {length1} and {length2} in format string '{fmt}'.")

        bits_per_group = None
        if fmt2 is not None and length2 is not None:
            bits_per_group = length2
        elif length1 is not None:
            bits_per_group = length1

        if bits_per_group is None:
            if fmt2 is None:
                bits_per_group = 8  # Default for 'bin' and 'hex'
                if name1 == 'oct':
                    bits_per_group = 12
                elif name1 == 'bytes':
                    bits_per_group = 32
            else:
                # Rule of thumb seems to work OK for all combinations.
                bits_per_group = 2 * bpc[name1] * bpc[name2]
                if bits_per_group >= 24:
                    bits_per_group //= 2

        format_sep = "   "  # String to insert on each line between multiple formats

        self._pp(name1, name2 if fmt2 is not None else None, bits_per_group, width, sep, format_sep, show_offset, stream, _lsb0, 1)
        return

    def copy(self: TBits) -> TBits:
        """Return a copy of the bitstring."""
        return self._copy()

    # Create native-endian functions as aliases depending on the byteorder
    if byteorder == 'little':
        _setfloatne = _setfloatle
        _readfloatne = _readfloatle
        _getfloatne = _getfloatle
        _setbfloatne = _setbfloatle
        _readbfloatne = _readbfloatle
        _getbfloatne = _getbfloatle
        _setuintne = _setuintle
        _readuintne = _readuintle
        _getuintne = _getuintle
        _setintne = _setintle
        _readintne = _readintle
        _getintne = _getintle
    else:
        _setfloatne = _setfloatbe
        _readfloatne = _readfloatbe
        _getfloatne = _getfloatbe
        _setbfloatne = _setbfloatbe
        _readbfloatne = _readbfloatbe
        _getbfloatne = _getbfloatbe
        _setuintne = _setuintbe
        _readuintne = _readuintbe
        _getuintne = _getuintbe
        _setintne = _setintbe
        _readintne = _readintbe
        _getintne = _getintbe

    # Dictionary that maps token names to the function that reads them
    _name_to_read: Dict[str, Callable[[Bits, int, int], Any]] = {
        'uint': _readuint,
        'u': _readuint,
        'uintle': _readuintle,
        'uintbe': _readuintbe,
        'uintne': _readuintne,
        'int': _readint,
        'i': _readint,
        'intle': _readintle,
        'intbe': _readintbe,
        'intne': _readintne,
        'float': _readfloatbe,
        'f': _readfloatbe,
        'floatbe': _readfloatbe,  # floatbe is a synonym for float
        'floatle': _readfloatle,
        'floatne': _readfloatne,
        'bfloat': _readbfloatbe,
        'bfloatbe': _readbfloatbe,
        'bfloatle': _readbfloatle,
        'bfloatne': _readbfloatne,
        'hex': _readhex,
        'h': _readhex,
        'oct': _readoct,
        'o': _readoct,
        'bin': _readbin,
        'b': _readbin,
        'bits': _readbits,
        'bytes': _readbytes,
        'ue': _readue,
        'se': _readse,
        'uie': _readuie,
        'sie': _readsie,
        'bool': _readbool,
        'pad': _readpad,
        'float8_143': _readfloat143,
        'float8_152': _readfloat152
    }

    # Mapping token names to the methods used to set them
    _setfunc: Dict[str, Callable[..., None]] = {
        'bin': _setbin_safe,
        'hex': _sethex,
        'oct': _setoct,
        'ue': _setue,
        'se': _setse,
        'uie': _setuie,
        'sie': _setsie,
        'bool': _setbool,
        'uint': _setuint,
        'int': _setint,
        'float': _setfloatbe,
        'bfloat': _setbfloatbe,
        'bfloatbe': _setbfloatbe,
        'bfloatle': _setbfloatle,
        'bfloatne': _setbfloatne,
        'uintbe': _setuintbe,
        'intbe': _setintbe,
        'floatbe': _setfloatbe,
        'uintle': _setuintle,
        'intle': _setintle,
        'floatle': _setfloatle,
        'uintne': _setuintne,
        'intne': _setintne,
        'floatne': _setfloatne,
        'bytes': _setbytes,
        'filename': _setfile,
        'bitarray': _setbitarray,
        'float8_152': _setfloat152,
        'float8_143': _setfloat143,
        'bits': _setbits
    }

    len = property(_getlength,
                   doc="""The length of the bitstring in bits. Read only.
                      """)
    length = property(_getlength,
                      doc="""The length of the bitstring in bits. Read only.
                      """)
    bool = property(_getbool,
                    doc="""The bitstring as a bool (True or False). Read only.
                    """)
    hex = property(_gethex,
                   doc="""The bitstring as a hexadecimal string. Read only.
                   """)
    bin = property(_getbin,
                   doc="""The bitstring as a binary string. Read only.
                   """)
    oct = property(_getoct,
                   doc="""The bitstring as an octal string. Read only.
                   """)
    bytes = property(_getbytes,
                     doc="""The bitstring as a bytes object. Read only.
                      """)
    int = property(_getint,
                   doc="""The bitstring as a two's complement signed int. Read only.
                      """)
    uint = property(_getuint,
                    doc="""The bitstring as a two's complement unsigned int. Read only.
                      """)
    float = property(_getfloatbe,
                     doc="""The bitstring as a big-endian floating point number. Read only.
                      """)
    bfloat = property(_getbfloatbe,
                      doc="""The bitstring as a 16 bit big-endian bfloat floating point number. Read only.
                      """)
    bfloatbe = property(_getbfloatbe,
                        doc="""The bitstring as a 16 bit big-endian bfloat floating point number. Read only.
                        """)
    bfloatle = property(_getbfloatle,
                        doc="""The bitstring as a 16 bit little-endian bfloat floating point number. Read only.
                        """)
    bfloatne = property(_getbfloatne,
                        doc="""The bitstring as a 16 bit native-endian bfloat floating point number. Read only.
                        """)
    intbe = property(_getintbe,
                     doc="""The bitstring as a two's complement big-endian signed int. Read only.
                     """)
    uintbe = property(_getuintbe,
                      doc="""The bitstring as a two's complement big-endian unsigned int. Read only.
                      """)
    floatbe = property(_getfloatbe,
                       doc="""The bitstring as a big-endian floating point number. Read only.
                      """)
    intle = property(_getintle,
                     doc="""The bitstring as a two's complement little-endian signed int. Read only.
                      """)
    uintle = property(_getuintle,
                      doc="""The bitstring as a two's complement little-endian unsigned int. Read only.
                      """)
    floatle = property(_getfloatle,
                       doc="""The bitstring as a little-endian floating point number. Read only.
                      """)
    intne = property(_getintne,
                     doc="""The bitstring as a two's complement native-endian signed int. Read only.
                      """)
    uintne = property(_getuintne,
                      doc="""The bitstring as a two's complement native-endian unsigned int. Read only.
                      """)
    floatne = property(_getfloatne,
                       doc="""The bitstring as a native-endian floating point number. Read only.
                      """)
    ue = property(_getue,
                  doc="""The bitstring as an unsigned exponential-Golomb code. Read only.
                      """)
    se = property(_getse,
                  doc="""The bitstring as a signed exponential-Golomb code. Read only.
                      """)
    uie = property(_getuie,
                   doc="""The bitstring as an unsigned interleaved exponential-Golomb code. Read only.
                      """)
    sie = property(_getsie,
                   doc="""The bitstring as a signed interleaved exponential-Golomb code. Read only.
                      """)
    float8_143 = property(_getfloat143,
                          doc="""The bitstring as an 8 bit float with float8_143 format. Read only.""")
    float8_152 = property(_getfloat152,
                          doc="""The bitstring as an 8 bit float with float8_152 format. Read only.""")

    # Some shortened aliases of the above properties
    i = int
    u = uint
    f = float
    b = bin
    o = oct
    h = hex


class BitArray(Bits):
    """A container holding a mutable sequence of bits.

    Subclass of the immutable Bits class. Inherits all of its
    methods (except __hash__) and adds mutating methods.

    Mutating methods:

    append() -- Append a bitstring.
    byteswap() -- Change byte endianness in-place.
    clear() -- Remove all bits from the bitstring.
    insert() -- Insert a bitstring.
    invert() -- Flip bit(s) between one and zero.
    overwrite() -- Overwrite a section with a new bitstring.
    prepend() -- Prepend a bitstring.
    replace() -- Replace occurrences of one bitstring with another.
    reverse() -- Reverse bits in-place.
    rol() -- Rotate bits to the left.
    ror() -- Rotate bits to the right.
    set() -- Set bit(s) to 1 or 0.

    Methods inherited from Bits:

    all() -- Check if all specified bits are set to 1 or 0.
    any() -- Check if any of specified bits are set to 1 or 0.
    copy() -- Return a copy of the bitstring.
    count() -- Count the number of bits set to 1 or 0.
    cut() -- Create generator of constant sized chunks.
    endswith() -- Return whether the bitstring ends with a sub-string.
    find() -- Find a sub-bitstring in the current bitstring.
    findall() -- Find all occurrences of a sub-bitstring in the current bitstring.
    join() -- Join bitstrings together using current bitstring.
    pp() -- Pretty print the bitstring.
    rfind() -- Seek backwards to find a sub-bitstring.
    split() -- Create generator of chunks split by a delimiter.
    startswith() -- Return whether the bitstring starts with a sub-bitstring.
    tobitarray() -- Return bitstring as a bitarray from the bitarray package.
    tobytes() -- Return bitstring as bytes, padding if needed.
    tofile() -- Write bitstring to file, padding if needed.
    unpack() -- Interpret bits using format string.

    Special methods:

    Mutating operators are available: [], <<=, >>=, +=, *=, &=, |= and ^=
    in addition to the inherited [], ==, !=, +, *, ~, <<, >>, &, | and ^.

    Properties:

    bin -- The bitstring as a binary string.
    hex -- The bitstring as a hexadecimal string.
    oct -- The bitstring as an octal string.
    bytes -- The bitstring as a bytes object.
    int -- Interpret as a two's complement signed integer.
    uint -- Interpret as a two's complement unsigned integer.
    float / floatbe -- Interpret as a big-endian floating point number.
    bool -- For single bit bitstrings, interpret as True or False.
    se -- Interpret as a signed exponential-Golomb code.
    ue -- Interpret as an unsigned exponential-Golomb code.
    sie -- Interpret as a signed interleaved exponential-Golomb code.
    uie -- Interpret as an unsigned interleaved exponential-Golomb code.
    floatle -- Interpret as a little-endian floating point number.
    floatne -- Interpret as a native-endian floating point number.
    bfloat / bfloatbe -- Interpret as a big-endian 16-bit bfloat type.
    bfloatle -- Interpret as a little-endian 16-bit bfloat type.
    bfloatne -- Interpret as a native-endian 16-bit bfloat type.
    intbe -- Interpret as a big-endian signed integer.
    intle -- Interpret as a little-endian signed integer.
    intne -- Interpret as a native-endian signed integer.
    uintbe -- Interpret as a big-endian unsigned integer.
    uintle -- Interpret as a little-endian unsigned integer.
    uintne -- Interpret as a native-endian unsigned integer.

    len -- Length of the bitstring in bits.

    """

    @classmethod
    def _setlsb0methods(cls, lsb0: bool) -> None:
        if lsb0:
            cls._ror = cls._rol_msb0  # type: ignore
            cls._rol = cls._ror_msb0  # type: ignore
            cls._append = cls._append_lsb0  # type: ignore
            # An LSB0 prepend is an MSB0 append
            cls._prepend = cls._append_msb0  # type: ignore
        else:
            cls._ror = cls._ror_msb0  # type: ignore
            cls._rol = cls._rol_msb0  # type: ignore
            cls._append = cls._append_msb0  # type: ignore
            cls._prepend = cls._append_lsb0  # type: ignore

    __slots__ = ()

    # As BitArray objects are mutable, we shouldn't allow them to be hashed.
    __hash__: None = None

    def __init__(self, __auto: Optional[Union[BitsType, int]] = None, length: Optional[int] = None,
                 offset: Optional[int] = None, **kwargs) -> None:
        """Either specify an 'auto' initialiser:
        A string of comma separated tokens, an integer, a file object,
        a bytearray, a boolean iterable or another bitstring.

        Or initialise via **kwargs with one (and only one) of:
        bin -- binary string representation, e.g. '0b001010'.
        hex -- hexadecimal string representation, e.g. '0x2ef'
        oct -- octal string representation, e.g. '0o777'.
        bytes -- raw data as a bytes object, for example read from a binary file.
        int -- a signed integer.
        uint -- an unsigned integer.
        float / floatbe -- a big-endian floating point number.
        bool -- a boolean (True or False).
        se -- a signed exponential-Golomb code.
        ue -- an unsigned exponential-Golomb code.
        sie -- a signed interleaved exponential-Golomb code.
        uie -- an unsigned interleaved exponential-Golomb code.
        floatle -- a little-endian floating point number.
        floatne -- a native-endian floating point number.
        bfloat / bfloatbe - a big-endian bfloat format 16-bit floating point number.
        bfloatle -- a little-endian bfloat format 16-bit floating point number.
        bfloatne -- a native-endian bfloat format 16-bit floating point number.
        intbe -- a signed big-endian whole byte integer.
        intle -- a signed little-endian whole byte integer.
        intne -- a signed native-endian whole byte integer.
        uintbe -- an unsigned big-endian whole byte integer.
        uintle -- an unsigned little-endian whole byte integer.
        uintne -- an unsigned native-endian whole byte integer.
        filename -- the path of a file which will be opened in binary read-only mode.

        Other keyword arguments:
        length -- length of the bitstring in bits, if needed and appropriate.
                  It must be supplied for all integer and float initialisers.
        offset -- bit offset to the data. These offset bits are
                  ignored and this is intended for use when
                  initialising using 'bytes' or 'filename'.

        """
        if self._bitstore.immutable:
            self._bitstore = self._bitstore.copy()
            self._bitstore.immutable = False

    _letter_to_setter: Dict[str, Callable[..., None]] = \
        {'u': Bits._setuint,
         'i': Bits._setint,
         'f': Bits._setfloatbe,
         'b': Bits._setbin_safe,
         'o': Bits._setoct,
         'h': Bits._sethex}

    _short_token: Pattern[str] = re.compile(r'^(?P<name>[uifboh])(?P<len>\d+)$', re.IGNORECASE)
    _name_length_pattern: Pattern[str] = re.compile(r'^(?P<name>[a-z]+)(?P<len>\d+)$', re.IGNORECASE)

    def __setattr__(self, attribute, value) -> None:
        try:
            # First try the ordinary attribute setter
            super().__setattr__(attribute, value)
        except AttributeError:
            m1_short = BitArray._short_token.match(attribute)
            if m1_short:
                length = int(m1_short.group('len'))
                name = m1_short.group('name')
                f = BitArray._letter_to_setter[name]
                try:
                    f(self, value, length)
                except AttributeError:
                    raise AttributeError(f"Can't set attribute {attribute} with value {value}.")

                if self.len != length:
                    new_len = self.len
                    raise CreationError(f"Can't initialise with value of length {new_len} bits, "
                                        f"as attribute has length of {length} bits.")
                return
            # Try to split into [name][length], then try standard properties
            name_length = BitArray._name_length_pattern.match(attribute)
            if name_length:
                name = name_length.group('name')
                length = name_length.group('len')
                if length is not None:
                    length = int(length)
                    if name == 'bytes':
                        if len(value) != length:
                            raise CreationError(
                                f"Wrong amount of byte data preset - {length} bytes needed, have {len(value)} bytes.")
                        length *= 8
                try:
                    self._initialise(None, length=length, offset=None, **{name: value})
                    return
                except AttributeError:
                    pass
            raise AttributeError(f"Can't set attribute {attribute} with value {value}.")

    def __iadd__(self, bs: BitsType) -> BitArray:
        """Append bs to current bitstring. Return self.

        bs -- the bitstring to append.

        """
        self._append(bs)
        return self

    def __copy__(self) -> BitArray:
        """Return a new copy of the BitArray."""
        s_copy = BitArray()
        s_copy._bitstore = self._bitstore.copy()
        assert s_copy._bitstore.immutable is False
        return s_copy

    def _setitem_int(self, key: int, value: Union[BitsType, int]) -> None:
        if isinstance(value, numbers.Integral):
            if value == 0:
                self._bitstore[key] = 0
                return
            if value in (1, -1):
                self._bitstore[key] = 1
                return
            raise ValueError(f"Cannot set a single bit with integer {value}.")
        try:
            value = self._create_from_bitstype(value)
        except TypeError:
            raise TypeError(f"Bitstring, integer or string expected. Got {type(value)}.")
        positive_key = key + self.len if key < 0 else key
        if positive_key < 0 or positive_key >= len(self._bitstore):
            raise IndexError(f"Bit position {key} out of range.")
        self._bitstore[positive_key: positive_key + 1] = value._bitstore

    def _setitem_slice(self, key: slice, value: BitsType) -> None:
        if isinstance(value, numbers.Integral):
            if key.step not in [None, -1, 1]:
                if value in [0, 1]:
                    self.set(value, range(*key.indices(len(self))))
                    return
                else:
                    raise ValueError("Can't assign an integer except 0 or 1 to a slice with a step value.")
            # To find the length we first get the slice
            s = self._bitstore.getslice(key)
            length = len(s)
            # Now create an int of the correct length
            if value >= 0:
                value = self.__class__(uint=value, length=length)
            else:
                value = self.__class__(int=value, length=length)
        else:
            try:
                value = self._create_from_bitstype(value)
            except TypeError:
                raise TypeError(f"Bitstring, integer or string expected. Got {type(value)}.")
        self._bitstore.__setitem__(key, value._bitstore)

    def __setitem__(self, key: Union[slice, int], value: BitsType) -> None:
        if isinstance(key, numbers.Integral):
            self._setitem_int(key, value)
        else:
            self._setitem_slice(key, value)

    def __delitem__(self, key: Union[slice, int]) -> None:
        """Delete item or range.

        >>> a = BitArray('0x001122')
        >>> del a[8:16]
        >>> print a
        0x0022

        """
        self._bitstore.__delitem__(key)
        return

    def __ilshift__(self: TBits, n: int) -> TBits:
        """Shift bits by n to the left in place. Return self.

        n -- the number of bits to shift. Must be >= 0.

        """
        if n < 0:
            raise ValueError("Cannot shift by a negative amount.")
        if not self.len:
            raise ValueError("Cannot shift an empty bitstring.")
        if not n:
            return self
        n = min(n, self.len)
        return self._ilshift(n)

    def __irshift__(self: TBits, n: int) -> TBits:
        """Shift bits by n to the right in place. Return self.

        n -- the number of bits to shift. Must be >= 0.

        """
        if n < 0:
            raise ValueError("Cannot shift by a negative amount.")
        if not self.len:
            raise ValueError("Cannot shift an empty bitstring.")
        if not n:
            return self
        n = min(n, self.len)
        return self._irshift(n)

    def __imul__(self: TBits, n: int) -> TBits:
        """Concatenate n copies of self in place. Return self.

        Called for expressions of the form 'a *= 3'.
        n -- The number of concatenations. Must be >= 0.

        """
        if n < 0:
            raise ValueError("Cannot multiply by a negative integer.")
        return self._imul(n)

    def __ior__(self: TBits, bs: BitsType) -> TBits:
        bs = self._create_from_bitstype(bs)
        if self.len != bs.len:
            raise ValueError("Bitstrings must have the same length for |= operator.")
        self._bitstore |= bs._bitstore
        return self

    def __iand__(self: TBits, bs: BitsType) -> TBits:
        bs = self._create_from_bitstype(bs)
        if self.len != bs.len:
            raise ValueError("Bitstrings must have the same length for &= operator.")
        self._bitstore &= bs._bitstore
        return self

    def __ixor__(self: TBits, bs: BitsType) -> TBits:
        bs = self._create_from_bitstype(bs)
        if self.len != bs.len:
            raise ValueError("Bitstrings must have the same length for ^= operator.")
        self._bitstore ^= bs._bitstore
        return self

    def _replace(self, old: Bits, new: Bits, start: int, end: int, count: int, bytealigned: Optional[bool]) -> int:
        if bytealigned is None:
            bytealigned = _bytealigned
        # First find all the places where we want to do the replacements
        starting_points: List[int] = []
        for x in self.findall(old, start, end, bytealigned=bytealigned):
            if not starting_points:
                starting_points.append(x)
            elif x >= starting_points[-1] + old.len:
                # Can only replace here if it hasn't already been replaced!
                starting_points.append(x)
            if count != 0 and len(starting_points) == count:
                break
        if not starting_points:
            return 0
        replacement_list = [self._bitstore.getslice(slice(0, starting_points[0], None))]
        for i in range(len(starting_points) - 1):
            replacement_list.append(new._bitstore)
            replacement_list.append(
                self._bitstore.getslice(slice(starting_points[i] + old.len, starting_points[i + 1], None)))
        # Final replacement
        replacement_list.append(new._bitstore)
        replacement_list.append(self._bitstore.getslice(slice(starting_points[-1] + old.len, None, None)))
        if _lsb0:
            # Addition of bitarray is always on the right, so assemble from other end
            replacement_list.reverse()
        self._bitstore.clear()
        for r in replacement_list:
            self._bitstore += r
        return len(starting_points)

    def replace(self, old: BitsType, new: BitsType, start: Optional[int] = None, end: Optional[int] = None,
                count: Optional[int] = None, bytealigned: Optional[bool] = None) -> int:
        """Replace all occurrences of old with new in place.

        Returns number of replacements made.

        old -- The bitstring to replace.
        new -- The replacement bitstring.
        start -- Any occurrences that start before this will not be replaced.
                 Defaults to 0.
        end -- Any occurrences that finish after this will not be replaced.
               Defaults to len(self).
        count -- The maximum number of replacements to make. Defaults to
                 replace all occurrences.
        bytealigned -- If True replacements will only be made on byte
                       boundaries.

        Raises ValueError if old is empty or if start or end are
        out of range.

        """
        if count == 0:
            return 0
        old = self._create_from_bitstype(old)
        new = self._create_from_bitstype(new)
        if not old.len:
            raise ValueError("Empty bitstring cannot be replaced.")
        start, end = self._validate_slice(start, end)

        if new is self:
            # Prevent self assignment woes
            new = copy.copy(self)
        return self._replace(old, new, start, end, 0 if count is None else count, bytealigned)

    def insert(self, bs: BitsType, pos: int) -> None:
        """Insert bs at bit position pos.

        bs -- The bitstring to insert.
        pos -- The bit position to insert at.

        Raises ValueError if pos < 0 or pos > len(self).

        """
        bs = self._create_from_bitstype(bs)
        if not bs.len:
            return
        if bs is self:
            bs = self._copy()
        if pos < 0:
            pos += self._getlength()
        if not 0 <= pos <= self._getlength():
            raise ValueError("Invalid insert position.")
        self._insert(bs, pos)

    def overwrite(self, bs: BitsType, pos: int) -> None:
        """Overwrite with bs at bit position pos.

        bs -- The bitstring to overwrite with.
        pos -- The bit position to begin overwriting from.

        Raises ValueError if pos < 0 or pos > len(self).

        """
        bs = self._create_from_bitstype(bs)
        if not bs.len:
            return
        if pos < 0:
            pos += self._getlength()
        if pos < 0 or pos > self.len:
            raise ValueError("Overwrite starts outside boundary of bitstring.")
        self._overwrite(bs, pos)

    def append(self, bs: BitsType) -> None:
        """Append a bitstring to the current bitstring.

        bs -- The bitstring to append.

        """
        self._append(bs)

    def prepend(self, bs: BitsType) -> None:
        """Prepend a bitstring to the current bitstring.

        bs -- The bitstring to prepend.

        """
        self._prepend(bs)

    def _append_msb0(self, bs: BitsType) -> None:
        self._addright(self._create_from_bitstype(bs))

    def _append_lsb0(self, bs: BitsType) -> None:
        bs = self._create_from_bitstype(bs)
        self._addleft(bs)

    def reverse(self, start: Optional[int] = None, end: Optional[int] = None) -> None:
        """Reverse bits in-place.

        start -- Position of first bit to reverse. Defaults to 0.
        end -- One past the position of the last bit to reverse.
               Defaults to len(self).

        Using on an empty bitstring will have no effect.

        Raises ValueError if start < 0, end > len(self) or end < start.

        """
        start, end = self._validate_slice(start, end)
        if start == 0 and end == self.len:
            self._bitstore.reverse()
            return
        s = self._slice(start, end)
        s._bitstore.reverse()
        self[start:end] = s

    def set(self, value: Any, pos: Optional[Union[int, Iterable[int]]] = None) -> None:
        """Set one or many bits to 1 or 0.

        value -- If bool(value) is True bits are set to 1, otherwise they are set to 0.
        pos -- Either a single bit position or an iterable of bit positions.
               Negative numbers are treated in the same way as slice indices.
               Defaults to the entire bitstring.

        Raises IndexError if pos < -len(self) or pos >= len(self).

        """
        if pos is None:
            # Set all bits to either 1 or 0
            self._setint(-1 if value else 0)
            return
        if not isinstance(pos, abc.Iterable):
            pos = (pos,)
        v = 1 if value else 0
        if isinstance(pos, range):
            self._bitstore.__setitem__(slice(pos.start, pos.stop, pos.step), v)
            return
        for p in pos:
            self._bitstore[p] = v

    def invert(self, pos: Optional[Union[Iterable[int], int]] = None) -> None:
        """Invert one or many bits from 0 to 1 or vice versa.

        pos -- Either a single bit position or an iterable of bit positions.
               Negative numbers are treated in the same way as slice indices.

        Raises IndexError if pos < -len(self) or pos >= len(self).

        """
        if pos is None:
            self._invert_all()
            return
        if not isinstance(pos, abc.Iterable):
            pos = (pos,)
        length = self.len

        for p in pos:
            if p < 0:
                p += length
            if not 0 <= p < length:
                raise IndexError(f"Bit position {p} out of range.")
            self._invert(p)

    def ror(self, bits: int, start: Optional[int] = None, end: Optional[int] = None) -> None:
        """Rotate bits to the right in-place.

        bits -- The number of bits to rotate by.
        start -- Start of slice to rotate. Defaults to 0.
        end -- End of slice to rotate. Defaults to len(self).

        Raises ValueError if bits < 0.

        """
        if not self.len:
            raise Error("Cannot rotate an empty bitstring.")
        if bits < 0:
            raise ValueError("Cannot rotate by negative amount.")
        self._ror(bits, start, end)

    def _ror_msb0(self, bits: int, start: Optional[int] = None, end: Optional[int] = None) -> None:
        start, end = self._validate_slice(start, end)  # the _slice deals with msb0/lsb0
        bits %= (end - start)
        if not bits:
            return
        rhs = self._slice(end - bits, end)
        self._delete(bits, end - bits)
        self._insert(rhs, start)

    def rol(self, bits: int, start: Optional[int] = None, end: Optional[int] = None) -> None:
        """Rotate bits to the left in-place.

        bits -- The number of bits to rotate by.
        start -- Start of slice to rotate. Defaults to 0.
        end -- End of slice to rotate. Defaults to len(self).

        Raises ValueError if bits < 0.

        """
        if not self.len:
            raise Error("Cannot rotate an empty bitstring.")
        if bits < 0:
            raise ValueError("Cannot rotate by negative amount.")
        self._rol(bits, start, end)

    def _rol_msb0(self, bits: int, start: Optional[int] = None, end: Optional[int] = None):
        start, end = self._validate_slice(start, end)
        bits %= (end - start)
        if bits == 0:
            return
        lhs = self._slice(start, start + bits)
        self._delete(bits, start)
        self._insert(lhs, end - bits)

    def byteswap(self, fmt: Optional[Union[int, Iterable[int], str]] = None, start: Optional[int] = None,
                 end: Optional[int] = None, repeat: bool = True) -> int:
        """Change the endianness in-place. Return number of repeats of fmt done.

        fmt -- A compact structure string, an integer number of bytes or
               an iterable of integers. Defaults to 0, which byte reverses the
               whole bitstring.
        start -- Start bit position, defaults to 0.
        end -- End bit position, defaults to len(self).
        repeat -- If True (the default) the byte swapping pattern is repeated
                  as much as possible.

        """
        start_v, end_v = self._validate_slice(start, end)
        if fmt is None or fmt == 0:
            # reverse all of the whole bytes.
            bytesizes = [(end_v - start_v) // 8]
        elif isinstance(fmt, numbers.Integral):
            if fmt < 0:
                raise ValueError(f"Improper byte length {fmt}.")
            bytesizes = [fmt]
        elif isinstance(fmt, str):
            m = BYTESWAP_STRUCT_PACK_RE.match(fmt)
            if not m:
                raise ValueError(f"Cannot parse format string {fmt}.")
            # Split the format string into a list of 'q', '4h' etc.
            formatlist = re.findall(STRUCT_SPLIT_RE, m.group('fmt'))
            # Now deal with multiplicative factors, 4h -> hhhh etc.
            bytesizes = []
            for f in formatlist:
                if len(f) == 1:
                    bytesizes.append(PACK_CODE_SIZE[f])
                else:
                    bytesizes.extend([PACK_CODE_SIZE[f[-1]]] * int(f[:-1]))
        elif isinstance(fmt, abc.Iterable):
            bytesizes = fmt
            for bytesize in bytesizes:
                if not isinstance(bytesize, numbers.Integral) or bytesize < 0:
                    raise ValueError(f"Improper byte length {bytesize}.")
        else:
            raise TypeError("Format must be an integer, string or iterable.")

        repeats = 0
        totalbitsize: int = 8 * sum(bytesizes)
        if not totalbitsize:
            return 0
        if repeat:
            # Try to repeat up to the end of the bitstring.
            finalbit = end_v
        else:
            # Just try one (set of) byteswap(s).
            finalbit = start_v + totalbitsize
        for patternend in range(start_v + totalbitsize, finalbit + 1, totalbitsize):
            bytestart = patternend - totalbitsize
            for bytesize in bytesizes:
                byteend = bytestart + bytesize * 8
                self._reversebytes(bytestart, byteend)
                bytestart += bytesize * 8
            repeats += 1
        return repeats

    def clear(self) -> None:
        """Remove all bits, reset to zero length."""
        self._clear()

    int = property(Bits._getint, Bits._setint,
                   doc="""The bitstring as a two's complement signed int. Read and write.
                      """)
    uint = property(Bits._getuint, Bits._setuint,
                    doc="""The bitstring as a two's complement unsigned int. Read and write.
                      """)
    float = property(Bits._getfloatbe, Bits._setfloatbe,
                     doc="""The bitstring as a floating point number. Read and write.
                      """)
    bfloat = property(Bits._getbfloatbe, Bits._setbfloatbe,
                      doc="""The bitstring as a 16 bit bfloat floating point number. Read and write.
                      """)
    intbe = property(Bits._getintbe, Bits._setintbe,
                     doc="""The bitstring as a two's complement big-endian signed int. Read and write.
                      """)
    uintbe = property(Bits._getuintbe, Bits._setuintbe,
                      doc="""The bitstring as a two's complement big-endian unsigned int. Read and write.
                      """)
    floatbe = property(Bits._getfloatbe, Bits._setfloatbe,
                       doc="""The bitstring as a big-endian floating point number. Read and write.
                      """)
    intle = property(Bits._getintle, Bits._setintle,
                     doc="""The bitstring as a two's complement little-endian signed int. Read and write.
                      """)
    uintle = property(Bits._getuintle, Bits._setuintle,
                      doc="""The bitstring as a two's complement little-endian unsigned int. Read and write.
                      """)
    floatle = property(Bits._getfloatle, Bits._setfloatle,
                       doc="""The bitstring as a little-endian floating point number. Read and write.
                      """)
    intne = property(Bits._getintne, Bits._setintne,
                     doc="""The bitstring as a two's complement native-endian signed int. Read and write.
                      """)
    uintne = property(Bits._getuintne, Bits._setuintne,
                      doc="""The bitstring as a two's complement native-endian unsigned int. Read and write.
                      """)
    floatne = property(Bits._getfloatne, Bits._setfloatne,
                       doc="""The bitstring as a native-endian floating point number. Read and write.
                      """)
    ue = property(Bits._getue, Bits._setue,
                  doc="""The bitstring as an unsigned exponential-Golomb code. Read and write.
                      """)
    se = property(Bits._getse, Bits._setse,
                  doc="""The bitstring as a signed exponential-Golomb code. Read and write.
                      """)
    uie = property(Bits._getuie, Bits._setuie,
                   doc="""The bitstring as an unsigned interleaved exponential-Golomb code. Read and write.
                      """)
    sie = property(Bits._getsie, Bits._setsie,
                   doc="""The bitstring as a signed interleaved exponential-Golomb code. Read and write.
                      """)
    hex = property(Bits._gethex, Bits._sethex,
                   doc="""The bitstring as a hexadecimal string. Read and write.
                       """)
    bin = property(Bits._getbin, Bits._setbin_safe,
                   doc="""The bitstring as a binary string. Read and write.
                       """)
    oct = property(Bits._getoct, Bits._setoct,
                   doc="""The bitstring as an octal string. Read and write.
                       """)
    bool = property(Bits._getbool, Bits._setbool,
                    doc="""The bitstring as a bool (True or False). Read and write.
                    """)
    bytes = property(Bits._getbytes, Bits._setbytes,
                     doc="""The bitstring as a ordinary string. Read and write.
                      """)
    float8_143 = property(Bits._getfloat143, Bits._setfloat143,
                          doc="""The bitstring as an 8 bit float with float8_143 format. Read and write.
                          """)
    float8_152 = property(Bits._getfloat152, Bits._setfloat152,
                          doc="""The bitstring as an 8 bit float with float8_152 format. Read and write.
                          """)

    # Aliases for some properties
    f = float
    i = int
    u = uint
    b = bin
    h = hex
    o = oct


# Whether to label the Least Significant Bit as bit 0. Default is False.

def _switch_lsb0_methods(lsb0: bool) -> None:
    global _lsb0
    _lsb0 = lsb0
    Bits._setlsb0methods(lsb0)
    BitArray._setlsb0methods(lsb0)
    BitStore._setlsb0methods(lsb0)


# Initialise the default behaviour
_switch_lsb0_methods(False)
