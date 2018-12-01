DEPS = []
INCL = []
PLAT = ['Linux', 'Darwin']
INFO = {'name': '', 'version': '0.1.1'}
LIBS = ['pthread', 'zmq', 'czmq', 'yaml']
DEFS = ['ERROR']
ARGS = {
    'QUIET': {'type': 'bool'},
    'DEBUG': {'type': 'bool'},
    'EVALUATE': {'type': 'bool'},
    'COUNTABLE': {'type': 'bool'},
    'CLONE_TS': {'type': 'bool'},
    'EVAL_ECHO': {'type': 'bool'},
    'EVAL_LATENCY': {'type': 'bool'},
    'EVAL_THROUGHPUT': {'type': 'bool'},
}
