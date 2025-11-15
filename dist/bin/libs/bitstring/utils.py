from __future__ import annotations
import itertools
import functools
import re
from typing import Tuple, List, Optional, Pattern, Dict, Union, Match
import sys
from bitstring.exceptions import Error

byteorder: str = sys.byteorder

CACHE_SIZE = 256

SIGNED_INTEGER_NAMES: List[str] = ['int', 'se', 'sie', 'intbe', 'intle', 'intne']
UNSIGNED_INTEGER_NAMES: List[str] = ['uint', 'ue', 'uie', 'uintbe', 'uintle', 'uintne', 'bool']
FLOAT_NAMES: List[str] = ['float', 'floatbe', 'floatle', 'floatne', 'bfloatbe', 'bfloatle', 'bfloatne', 'bfloat', 'float8_143', 'float8_152']
STRING_NAMES: List[str] = ['hex', 'oct', 'bin']

INIT_NAMES: List[str] = SIGNED_INTEGER_NAMES  + UNSIGNED_INTEGER_NAMES + FLOAT_NAMES + STRING_NAMES + ['bits', 'bytes', 'pad']
# Sort longest first as we want to match them in that order (so floatne before float etc.).
INIT_NAMES.sort(key=len, reverse=True)

TOKEN_RE: Pattern[str] = re.compile(r'^(?P<name>' + '|'.join(INIT_NAMES) +
                                    r'):?(?P<len>[^=]+)?(=(?P<value>.*))?$', re.IGNORECASE)
# Tokens such as 'u32', 'f64=4.5' or 'i6=-3'
SHORT_TOKEN_RE: Pattern[str] = re.compile(r'^(?P<name>[uifboh]):?(?P<len>\d+)?(=(?P<value>.*))?$')
DEFAULT_BITS: Pattern[str] = re.compile(r'^(?P<len>[^=]+)?(=(?P<value>.*))?$', re.IGNORECASE)

# A string followed by optional : then an integer number
STR_INT_RE: Pattern[str] = re.compile(r'^(?P<string>.+?):?(?P<integer>\d*)$')

MULTIPLICATIVE_RE: Pattern[str] = re.compile(r'^(?P<factor>.*)\*(?P<token>.+)')

# Hex, oct or binary literals
LITERAL_RE: Pattern[str] = re.compile(r'^(?P<name>0([xob]))(?P<value>.+)', re.IGNORECASE)

# An endianness indicator followed by one or more struct.pack codes
STRUCT_PACK_RE: Pattern[str] = re.compile(r'^(?P<endian>[<>@=]){1}(?P<fmt>(?:\d*[bBhHlLqQefd])+)$')
# The same as above, but it doesn't insist on an endianness as it's byteswapping anyway.
BYTESWAP_STRUCT_PACK_RE: Pattern[str] = re.compile(r'^(?P<endian>[<>@=])?(?P<fmt>(?:\d*[bBhHlLqQefd])+)$')
# An endianness indicator followed by exactly one struct.pack codes
SINGLE_STRUCT_PACK_RE: Pattern[str] = re.compile(r'^(?P<endian>[<>@=]){1}(?P<fmt>(?:[bBhHlLqQefd]))$')

# A number followed by a single character struct.pack code
STRUCT_SPLIT_RE: Pattern[str] = re.compile(r'\d*[bBhHlLqQefd]')

# These replicate the struct.pack codes
# Big-endian
REPLACEMENTS_BE: Dict[str, str] = {'b': 'int:8', 'B': 'uint:8',
                                   'h': 'intbe:16', 'H': 'uintbe:16',
                                   'l': 'intbe:32', 'L': 'uintbe:32',
                                   'q': 'intbe:64', 'Q': 'uintbe:64',
                                   'e': 'floatbe:16', 'f': 'floatbe:32', 'd': 'floatbe:64'}
# Little-endian
REPLACEMENTS_LE: Dict[str, str] = {'b': 'int:8', 'B': 'uint:8',
                                   'h': 'intle:16', 'H': 'uintle:16',
                                   'l': 'intle:32', 'L': 'uintle:32',
                                   'q': 'intle:64', 'Q': 'uintle:64',
                                   'e': 'floatle:16', 'f': 'floatle:32', 'd': 'floatle:64'}

# Native-endian
REPLACEMENTS_NE: Dict[str, str] = {'b': 'int:8', 'B': 'uint:8',
                                   'h': 'intne:16', 'H': 'uintne:16',
                                   'l': 'intne:32', 'L': 'uintne:32',
                                   'q': 'intne:64', 'Q': 'uintne:64',
                                   'e': 'floatne:16', 'f': 'floatne:32', 'd': 'floatne:64'}

# Tokens which are always the same length, so it doesn't need to be supplied.
FIXED_LENGTH_TOKENS: Dict[str, int] = {'bool': 1,
                                       'bfloat': 16,
                                       'float8_143': 8,
                                       'float8_152': 8}

def structparser(m: Match[str]) -> List[str]:
    """Parse struct-like format string token into sub-token list."""
    endian = m.group('endian')
    # Split the format string into a list of 'q', '4h' etc.
    formatlist = re.findall(STRUCT_SPLIT_RE, m.group('fmt'))
    # Now deal with multiplicative factors, 4h -> hhhh etc.
    fmt = ''.join([f[-1] * int(f[:-1]) if len(f) != 1 else
                   f for f in formatlist])
    if endian in '@=':
        # Native endianness
        tokens = [REPLACEMENTS_NE[c] for c in fmt]
    elif endian == '<':
        tokens = [REPLACEMENTS_LE[c] for c in fmt]
    else:
        assert endian == '>'
        tokens = [REPLACEMENTS_BE[c] for c in fmt]
    return tokens

@functools.lru_cache(CACHE_SIZE)
def parse_name_length_token(fmt: str) -> Tuple[str, int]:
    # Any single token with just a name and length
    m = SINGLE_STRUCT_PACK_RE.match(fmt)
    if m:
        endian = m.group('endian')
        f = m.group('fmt')
        if endian == '>':
            fmt = REPLACEMENTS_BE[f]
        elif endian == '<':
            fmt = REPLACEMENTS_LE[f]
        else:
            assert endian in '=@'
            fmt = REPLACEMENTS_NE[f]
    m2 = STR_INT_RE.match(fmt)
    if m2:
        name = m2.group('string')
        length_str = m2.group('integer')
        length = 0 if length_str == '' else int(length_str)
    else:
        raise ValueError(f"Can't parse 'name[:]length' token '{fmt}'.")
    if name in 'uifboh':
        name = {'u': 'uint',
                'i': 'int',
                'f': 'float',
                'b': 'bin',
                'o': 'oct',
                'h': 'hex'}[name]
    if name in ('se', 'ue', 'sie', 'uie'):
        if length is not None:
            raise ValueError(
                f"Exponential-Golomb codes (se/ue/sie/uie) can't have fixed lengths. Length of {length} was given.")

    if name == 'float8_':
        name += str(length)
        length = 8

    if name in FIXED_LENGTH_TOKENS.keys():
        token_length = FIXED_LENGTH_TOKENS[name]
        if length not in [0, token_length]:
            raise ValueError(f"{name} tokens can only be {token_length} bits long, not {length} bits.")
        length = token_length

    if length is None:
        length = 0
    return name, length

@functools.lru_cache(CACHE_SIZE)
def parse_single_token(token: str) -> Tuple[str, str, Optional[str]]:
    m1 = TOKEN_RE.match(token)
    if m1:
        name = m1.group('name')
        length = m1.group('len')
        value = m1.group('value')
    else:
        m1_short = SHORT_TOKEN_RE.match(token)
        if m1_short:
            name = m1_short.group('name')
            name = {'u': 'uint',
                    'i': 'int',
                    'f': 'float',
                    'b': 'bin',
                    'o': 'oct',
                    'h': 'hex'}[name]
            length = m1_short.group('len')
            value = m1_short.group('value')
        else:
            # If you don't specify a 'name' then the default is 'bits':
            name = 'bits'
            m2 = DEFAULT_BITS.match(token)
            if not m2:
                raise ValueError(f"Don't understand token '{token}'.")
            length = m2.group('len')
            value = m2.group('value')

    if name in FIXED_LENGTH_TOKENS.keys():
        token_length = str(FIXED_LENGTH_TOKENS[name])
        if length is not None and length != token_length:
            raise ValueError(f"{name} tokens can only be {token_length} bits long, not {length} bits.")
        length = token_length

    return name, length, value


@functools.lru_cache(CACHE_SIZE)
def tokenparser(fmt: str, keys: Tuple[str, ...] = ()) -> \
        Tuple[bool, List[Tuple[str, Union[int, str, None], Optional[str]]]]:
    """Divide the format string into tokens and parse them.

    Return stretchy token and list of [initialiser, length, value]
    initialiser is one of: hex, oct, bin, uint, int, se, ue, 0x, 0o, 0b etc.
    length is None if not known, as is value.

    If the token is in the keyword dictionary (keys) then it counts as a
    special case and isn't messed with.

    tokens must be of the form: [factor*][initialiser][:][length][=value]

    """
    # Remove whitespace
    fmt = ''.join(fmt.split())
    # Expand any brackets.
    fmt = expand_brackets(fmt)
    # Split tokens by ',' and remove whitespace
    # The meta_tokens can either be ordinary single tokens or multiple
    # struct-format token strings.
    meta_tokens = [f.strip() for f in fmt.split(',')]
    return_values: List[Tuple[str, Union[int, str, None], Optional[str]]] = []
    stretchy_token = False
    for meta_token in meta_tokens:
        # See if it has a multiplicative factor
        m = MULTIPLICATIVE_RE.match(meta_token)
        if not m:
            factor = 1
        else:
            factor = int(m.group('factor'))
            meta_token = m.group('token')
        # See if it's a struct-like format
        m = STRUCT_PACK_RE.match(meta_token)
        if m:
            tokens = structparser(m)
        else:
            tokens = [meta_token]
        ret_vals: List[Tuple[str, Union[str, int, None], Optional[str]]] = []
        for token in tokens:
            if keys and token in keys:
                # Don't bother parsing it, it's a keyword argument
                ret_vals.append((token, None, None))
                continue
            if token == '':
                continue

            # Match literal tokens of the form 0x... 0o... and 0b...
            m = LITERAL_RE.match(token)
            if m:
                ret_vals.append((m.group('name'), None, m.group('value')))
                continue

            name, length, value = parse_single_token(token)

            if name in ('se', 'ue', 'sie', 'uie'):
                if length is not None:
                    raise ValueError(f"Exponential-Golomb codes (se/ue/sie/uie) can't have fixed lengths. Length of {length} was given.")
            else:
                if length is None:
                    stretchy_token = True

            if length is not None:
                # Try converting length to int, otherwise check it's a key.
                try:
                    length = int(length)
                    if length < 0:
                        raise Error
                    # For the 'bytes' token convert length to bits.
                    if name == 'bytes':
                        length *= 8
                except Error:
                    raise ValueError("Can't read a token with a negative length.")
                except ValueError:
                    if not keys or length not in keys:
                        raise ValueError(f"Don't understand length '{length}' of token.")
            ret_vals.append((name, length, value))
        return_values.extend(itertools.repeat(ret_vals, factor))

    return_values = itertools.chain.from_iterable(return_values)
    return stretchy_token, list(return_values)


def expand_brackets(s: str) -> str:
    """Expand all brackets."""
    while True:
        start = s.find('(')
        if start == -1:
            break
        count = 1  # Number of hanging open brackets
        p = start + 1
        while p < len(s):
            if s[p] == '(':
                count += 1
            if s[p] == ')':
                count -= 1
            if not count:
                break
            p += 1
        if count:
            raise ValueError(f"Unbalanced parenthesis in '{s}'.")
        if start == 0 or s[start - 1] != '*':
            s = s[0:start] + s[start + 1:p] + s[p + 1:]
        else:
            # Looks for first number*(
            bracket_re = re.compile(r'(?P<factor>\d+)\*\(')
            m = bracket_re.search(s)
            if m:
                factor = int(m.group('factor'))
                matchstart = m.start('factor')
                s = s[0:matchstart] + (factor - 1) * (s[start + 1:p] + ',') + s[start + 1:p] + s[p + 1:]
            else:
                raise ValueError(f"Failed to parse '{s}'.")
    return s
