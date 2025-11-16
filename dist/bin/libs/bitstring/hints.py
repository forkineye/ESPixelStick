
from typing import List, Iterable


class ArrayOfThings:

    def __init__(self, data):
        self.data = data

    def update_values(self, delta):
        if len(delta) != len(self.data):
            raise ValueError("Dimensions don't match!")
        for i in range(len(self.data)):
            self.data[i] += delta[i]



from bitstring import BitField

header_field = BitField(['int4', 'bool', 'uint3'])
header = a.read(header_field)
a[0]
a[1]
a[2]
