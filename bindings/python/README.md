## Python Bindings for mpibind

### Overview

You can interact with mpibind through your python scripts:

```python
# running on syrah login node
from mpibind import MpibindWrapper
wrapper = MpibindWrapper()
mapping = wrapper.get_mapping(ntasks=4)
```

### Installing

### License

*mpibind* is distributed under the terms of the MIT license. All new
contributions must be made under this license. 

See [LICENSE](LICENSE) and [NOTICE](NOTICE) for details.

SPDX-License-Identifier: MIT.

LLNL-CODE-812647.
