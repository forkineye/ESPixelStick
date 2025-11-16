from __future__ import annotations

import functools
from bitstring.utils import parse_name_length_token, SIGNED_INTEGER_NAMES, UNSIGNED_INTEGER_NAMES, FLOAT_NAMES, FIXED_LENGTH_TOKENS
from bitstring.exceptions import InterpretError
from bitstring.classes import Bits, BitArray

INTEGER_NAMES = SIGNED_INTEGER_NAMES + UNSIGNED_INTEGER_NAMES
SIGNED_NAMES = SIGNED_INTEGER_NAMES + FLOAT_NAMES


class Dtype:

    # __slots__ = ('name', 'length', 'set', 'get', 'is_integer', 'is_signed', 'is_float', 'is_fixedlength')

    def __init__(self, fmt: str) -> None:
        self.name, self.length = parse_name_length_token(fmt)
        try:
            self.set = functools.partial(Bits._setfunc[self.name], length=self.length)
        except KeyError:
            raise ValueError(f"The token '{self.name}' is not a valid Dtype name.")
        try:
            self.get = functools.partial(Bits._name_to_read[self.name], length=self.length)
        except KeyError:
            raise ValueError(f"The token '{self.name}' is not a valid Dtype name.")

        # Test if the length makes sense by trying out the getter.
        if self.length != 0:
            temp = BitArray(self.length)
            try:
                _ = self.get(temp, 0)
            except InterpretError as e:
                raise ValueError(f"Invalid Dtype: {e.msg}")

        # Some useful information about the type we're creating
        self.is_integer = self.name in INTEGER_NAMES
        self.is_signed = self.name in SIGNED_NAMES
        self.is_float = self.name in FLOAT_NAMES
        self.is_fixedlength = self.name in FIXED_LENGTH_TOKENS.keys()

        self.max_value = None
        self.min_value = None
        if self.is_integer:
            if self.is_signed:
                self.max_value = (1 << (self.length - 1)) - 1
                self.min_value = -(1 << (self.length - 1))
            else:
                self.max_value = (1 << self.length) - 1
                self.min_value = 0

    def __str__(self) -> str:
        length_str = '' if (self.length == 0 or self.is_fixedlength) else str(self.length)
        return f"{self.name}{length_str}"

    def __repr__(self) -> str:
        s = self.__str__()
        return f"{self.__class__.__name__}('{s}')"

