
from bitstring import Dtype

# How would I like the user interface to be when creating a new data type?

def getuintr(length: int) -> int:

reverse_int = Dtype(name='uintr', getter=getuintr, setter=setuintr)




class Dtype:

    def get_uint(self, length):
        pass
    def get_bin(self, length):
        pass
    def get_float(self, length):
        pass
    def get_bfloat(self):
        pass
    def get_se(self):
        pass
    def get_bits(self, length):
        pass

    def set_uint(self, length, value) -> Bits:
        pass


class Reverse_int(Dtype):

    def get(self, length: int) -> int:
        x = self.get_bits(length)
        x.reverse()
        return x.uint

    def set(self, value: int, length: int) -> Bits:
        x = self.set_uint(value, length)
        x.reverse()
        return x


