import claripy

from binary import bitsizes # ugh! TODO remove need for this

"""
The original peer symbex approach from https://hoheinzollern.files.wordpress.com/2008/04/seer1.pdf works for exhaustive symbex of all boolean conditions.
We extend it to support an 'or' combination for any value, so that one can say symbex succeeds if any one of a given set of choices succeeds.
This provides a form of existential quantification to the code under symbex, similar to https://www.usenix.org/conference/hotos13/session/bugnion

The code under verification can call the '__choose__(choices)' function, which returns a choice, to use existential quantification.

The "run" function executes one path through the code, controllable with the following global variables:
- `__branches__`, list of (formula, bool) tuples, where the formula is the branch condition and the bool is its assignment for the path
- `__choices__`, list of lists, where the first item is the one that is used and the remaining are the alternatives
In both cases, they can be pre-populated to force a specific path prefix, and will contain the entirety of the path at the end

Additional global variables:
- `__state__`, the current state

Private global variables:
- `__branch_index__`, the index in `__branches__`
- `__choice_index__`, the index in `__choices__`
"""

def verif_builtin_choose(choices):
    global __choices__, __choice_index__
    if __choice_index__ == len(__choices__):
        __choices__.append(choices)
    result = __choices__[__choice_index__][0]
    __choice_index__ = __choice_index__ + 1
    return result
    

def verif_builtin_type_size(type):
    if isinstance(type, int):
        return type
    if isinstance(type, str):
        return getattr(bitsizes, type)
    if isinstance(type, dict):
        return sum([verif_builtin_type_size(v) for v in type.values()])
    raise Exception(f"idk what to do with type '{type}'")


class ValueProxy:
    @staticmethod
    def unwrap(value, type=None):
        if not isinstance(value, ValueProxy):
            return value
        result = value._value
        if type is None:
            return result
        else:
            size = verif_builtin_type_size(type)
            assert size >= result.size(), "the actual type should have a size at least that of the result's"
            return result.zero_extend(size - result.size())

    @staticmethod
    def wrap_func(func):
        return lambda *args: ValueProxy(func(*[ValueProxy.unwrap(arg) for arg in args]))

    def __init__(self, value, type=None):
        assert value is not None and value is not NotImplemented
        assert not isinstance(value, ValueProxy)
        self._value = value
        self._type = type
        if self._type is not None:
            size = verif_builtin_type_size(self._type)
            assert size <= self._value.size(), "the actual type should have a size at most that of the result's"
            if size < self._value.size():
                self._value = self._value[size-1:0]

    def __getattr__(self, name):
        if name[0] == "_":
            # Private members, for use within the class itself
            return super().__getattr__(name, value)

        if isinstance(self._type, dict):
            if name in self._type:
                offset = 0
                for (k, v) in self._type.items(): # Python preserves insertion order from 3.7 (3.6 for CPython)
                    if k == name:
                        return ValueProxy(self._value[verif_builtin_type_size(v)+offset-1:offset], type=v)
                    offset = offset + verif_builtin_type_size(v)

        # Only forward attrs if we're not a Claripy instance
        if not isinstance(self._value, claripy.ast.Base) and hasattr(self._value, name):
            return ValueProxy(getattr(self._value, name))

        raise Exception(f"idk what to do about attr '{name}'")

    def __setattr__(self, name, value):
        assert name[0] == "_", "can only set private variables, which should be from within the class itself"
        return super().__setattr__(name, value)
    
    def __str__(self):
        return self._value.__str__()

    def __repr__(self):
        return self._value.__repr__()

    def _op(self, other, op):
        if isinstance(self._type, dict):
            raise Exception("Cannot perform ops on a composite type")

        other_value = other
        self_value = self._value

        # Convert if needed
        if isinstance(other, ValueProxy):
            other_value = other._value
        if isinstance(other_value, float) and other_value == other_value // 1:
            other_value = int(other_value)
        if not isinstance(other_value, claripy.ast.Base):
            if isinstance(other_value, int):
                other_value = claripy.BVV(other_value, max(8, self_value.size())) # 8 bits minimum

        if isinstance(self_value, claripy.ast.BV):
            self_value = self_value.zero_extend(max(0, other_value.size() - self_value.size()))
            other_value = other_value.zero_extend(max(0, self_value.size() - other_value.size()))

        return ValueProxy(getattr(self_value, op)(other_value))


    def __bool__(self):
        if not isinstance(self._value, claripy.ast.Base):
            return bool(self._value)

        assert isinstance(self._value, claripy.ast.Bool)

        global __branches__, __branch_index__, __state__

        path_condition = [f if b else ~f for (f, b) in __branches__[:__branch_index__]]
        outcomes = __state__.solver.eval_upto(self._value, 3, extra_constraints=path_condition) # ask for 3 just in case something goes wrong; we want 1 or 2

        if len(outcomes) == 1:
            return outcomes[0]

        assert len(outcomes) == 2

        if __branch_index__ == len(__branches__):
            __branches__.append((self._value, True))
        result = __branches__[__branch_index__][1]
        __branch_index__ = __branch_index__ + 1
        return result

    
    def __contains__(self, item):
        assert not isinstance(self._value, claripy.ast.Base)
        return item in self._value

    def __invert__(self):
        return ValueProxy(~self._value)

    def __getitem__(self, item):
        return ValueProxy(self._value[item])

    def __and__(self, other):
        return self._op(other, "__and__")
    def __rand__(self, other):
        return self._op(other, "__and__")
    
    def __or__(self, other):
        return self._op(other, "__or__")
    def __ror__(self, other):
        return self._op(other, "__or__")

    def __eq__(self, other):
        return self._op(other, "__eq__")

    def __ne__(self, other):
        return self._op(other, "__ne__")

    def __lt__(self, other):
        return self._op(other, "__lt__") # TODO: signedness of {L/G}{E/T} and rshift

    def __le__(self, other):
        return self._op(other, "__le__")

    def __gt__(self, other):
        return self._op(other, "__gt__")

    def __ge__(self, other):
        return self._op(other, "__ge__")
    
    def __add__(self, other):
        return self._op(other, "__add__")
    def __radd__(self, other):
        return self._op(other, "__add__")
    
    def __mul__(self, other):
        return self._op(other, "__mul__")
    def __rmul__(self, other):
        return self._op(other, "__mul__")
    
    def __rshift__(self, other):
        return self._op(other, "LShR")
    
    def __lshift__(self, other):
        return self._op(other, "__lshift__")


def _symbex_one(state, func, branches, choices):
    global __branches__, __branch_index__, __choices__, __choice_index__, __state__
    __branches__ = branches
    __branch_index__ = 0
    __choices__ = choices
    __choice_index__ = 0
    __state__ = state
    func()
    return __branches__[:__branch_index__], __choices__[:__choice_index__]

def _symbex(state, func):
    branches = []
    while True:
        choices = []
        while True:
            try:
                (branches, choices) = _symbex_one(state, func, branches, choices)
                break # yay, we found a good set of choices!
            except:
                # Prune choice sets that were fully explored
                while len(choices) > 0 and len(choices[-1]) == 1: choices.pop()
                # If all choices were explored, we failed
                if len(choices) == 0: raise
                # Otherwise, change the last choice
                choices[-1].pop(0)
        
        # Path succeeded
        yield ([f if b else ~f for (f, b) in branches], [cs[0] for cs in choices])

        # Prune branches that were fully explored
        while len(branches) > 0 and not branches[-1][1]: branches.pop()
        # If all branches were fully explored, we're done
        if len(branches) == 0: return
        # Otherwise, flip the last branch
        branches[-1] = False

__branches__, __branch_index__, __choices__, __choice_index__, __state__ = None, None, None, None, None
def symbex(state, program, func_name, args, _globs):
    globs = globals()
    globs.update(_globs)
    globs['__choose__'] = verif_builtin_choose
    globs['__type_size__'] = verif_builtin_type_size
    globs = {k: (ValueProxy.wrap_func(v) if callable(v) else v) for (k, v) in globs.items()}
    args = [ValueProxy(arg) for arg in args]
    # locals have to be the same as globals, otherwise Python encapsulates the program in a class and then one can't use classes inside it...
    exec(program, globs, globs)
    return _symbex(state, lambda: globs[func_name](*args))
