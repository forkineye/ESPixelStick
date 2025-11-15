from __future__ import annotations

import bitarray
from bitstring.exceptions import CreationError
from typing import Union, Iterable, Optional, overload


def _offset_slice_indices_lsb0(key: slice, length: int, offset: int) -> slice:
    # First convert slice to all integers
    # Length already should take account of the offset
    start, stop, step = key.indices(length)
    new_start = length - stop - offset
    new_stop = length - start - offset
    # For negative step we sometimes get a negative stop, which can't be used correctly in a new slice
    return slice(new_start, None if new_stop < 0 else new_stop, step)


def _offset_slice_indices_msb0(key: slice, length: int, offset: int) -> slice:
    # First convert slice to all integers
    # Length already should take account of the offset
    start, stop, step = key.indices(length)
    start += offset
    stop += offset
    # For negative step we sometimes get a negative stop, which can't be used correctly in a new slice
    return slice(start, None if stop < 0 else stop, step)


class BitStore(bitarray.bitarray):
    """A light wrapper around bitarray that does the LSB0 stuff"""

    __slots__ = ('modified', 'length', 'offset', 'filename', 'immutable')

    def __init__(self, *args, immutable: bool = False, frombytes: Optional[Union[bytes, bytearray]] = None,
                 offset: int = 0, length: Optional[int] = None, filename: str = '',
                 **kwargs) -> None:
        if frombytes is not None:
            self.frombytes(frombytes)
        self.immutable = immutable
        self.offset = offset
        self.filename = filename
        # Here 'modified' means that it isn't just the underlying bitarray. It could have a different start and end, and be from a file.
        # This also means that it shouldn't be changed further, so setting deleting etc. are disallowed.
        self.modified = offset != 0 or length is not None or filename != ''
        if self.modified:
            assert immutable is True
            # These class variable only exist if modified is True.

            self.length = super().__len__() - self.offset if length is None else length

            if self.length < 0:
                raise CreationError("Can't create bitstring with a negative length.")
            if self.length + self.offset > super().__len__():
                self.length = super().__len__() - self.offset
                raise CreationError(
                    f"Can't create bitstring with a length of {self.length} and an offset of {self.offset} from {super().__len__()} bits of data.")

    @classmethod
    def _setlsb0methods(cls, lsb0: bool = False) -> None:
        if lsb0:
            cls.__setitem__ = cls.setitem_lsb0
            cls.__delitem__ = cls.delitem_lsb0
            cls.getindex = cls.getindex_lsb0
            cls.getslice = cls.getslice_lsb0
            cls.invert = cls.invert_lsb0
        else:
            cls.__setitem__ = super().__setitem__
            cls.__delitem__ = super().__delitem__
            cls.getindex = cls.getindex_msb0
            cls.getslice = cls.getslice_msb0
            cls.invert = cls.invert_msb0

    def __new__(cls, *args, **kwargs) -> bitarray.bitarray:
        # Just pass on the buffer keyword, not the length, offset, filename and frombytes
        new_kwargs = {'buffer': kwargs.get('buffer', None)}
        return bitarray.bitarray.__new__(cls, *args, **new_kwargs)

    def __add__(self, other: bitarray.bitarray) -> BitStore:
        assert not self.immutable
        return BitStore(super().__add__(other))

    def __iter__(self) -> Iterable[bool]:
        for i in range(len(self)):
            yield self.getindex(i)

    def copy(self) -> BitStore:
        x = BitStore(self.getslice(slice(None, None, None)))
        return x

    def __getitem__(self, item: Union[int, slice]) -> Union[int, BitStore]:
        # Use getindex or getslice instead
        raise NotImplementedError

    def getindex_msb0(self, index: int) -> bool:
        if self.modified and index >= 0:
            index += self.offset
        return bool(super().__getitem__(index))

    def getslice_msb0(self, key: slice) -> BitStore:
        if self.modified:
            key = _offset_slice_indices_msb0(key, len(self), self.offset)
        return BitStore(super().__getitem__(key))

    def getindex_lsb0(self, index: int) -> bool:
        if self.modified and index >= 0:
            index += self.offset
        return bool(super().__getitem__(-index - 1))

    def getslice_lsb0(self, key: slice) -> BitStore:
        if self.modified:
            key = _offset_slice_indices_lsb0(key, len(self), self.offset)
        else:
            key = _offset_slice_indices_lsb0(key, len(self), 0)
        return BitStore(super().__getitem__(key))

    @overload
    def setitem_lsb0(self, key: int, value: int) -> None:
        ...

    @overload
    def setitem_lsb0(self, key: slice, value: BitStore) -> None:
        ...

    def setitem_lsb0(self, key: Union[int, slice], value: Union[int, BitStore]) -> None:
        assert not self.immutable
        if isinstance(key, slice):
            new_slice = _offset_slice_indices_lsb0(key, len(self), 0)
            super().__setitem__(new_slice, value)
        else:
            super().__setitem__(-key - 1, value)

    def delitem_lsb0(self, key: Union[int, slice]) -> None:
        assert not self.immutable
        if isinstance(key, slice):
            new_slice = _offset_slice_indices_lsb0(key, len(self), 0)
            super().__delitem__(new_slice)
        else:
            super().__delitem__(-key - 1)

    def invert_msb0(self, index: Optional[int] = None) -> None:
        assert not self.immutable
        if index is not None:
            super().invert(index)
        else:
            super().invert()

    def invert_lsb0(self, index: Optional[int] = None) -> None:
        assert not self.immutable
        if index is not None:
            super().invert(-index - 1)
        else:
            super().invert()

    def any_set(self) -> bool:
        if self.modified:
            return super().__getitem__(slice(self.offset, self.offset + self.length, None)).any()
        else:
            return super().any()

    def all_set(self) -> bool:
        if self.modified:
            return super().__getitem__(slice(self.offset, self.offset + self.length, None)).all()
        else:
            return super().all()

    def __len__(self) -> int:
        if self.modified:
            return self.length
        return super().__len__()

    # Default to the MSB0 methods (mainly to stop mypy from complaining)
    getslice = getslice_msb0
    getindex = getindex_msb0
    __setitem__ = bitarray.bitarray.__setitem__
    __delitem__ = bitarray.bitarray.__delitem__