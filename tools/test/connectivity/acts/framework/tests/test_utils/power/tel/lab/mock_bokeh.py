import sys
from mock import Mock

sys.modules['bokeh'] = Mock()
sys.modules['bokeh.layouts'] = Mock()
sys.modules['bokeh.models'] = Mock()
sys.modules['bokeh.models.widgets'] = Mock()
sys.modules['bokeh.plotting'] = Mock()