from .wrapper import Wrapper
from _mpibind._mpibind import ffi, lib
import io
from contextlib import redirect_stdout
import re

class MpibindWrapper(Wrapper):
    def __init__(self):
        super().__init__(ffi, lib, prefixes=("mpibind_",))