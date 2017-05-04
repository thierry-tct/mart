# Numeric Expression operator classes

This directory contain:
    - The base abstract class implemented by the mutation operator for Expression. Example: mutating `x+1` into `(x/3)+1` or `x-3` into `x-(3/1)`, where _x_ is a numeric expression (including var and const). 
    - The subclasses make use of Numeric Constant operator classes by calling the match and replace method of the constant classes through const op object in UserMaps, knowing the corresponding ExpElemKey (mANYICONST and mANYFCONST).
    - The directory operator contains the mutation operators implementing this base class.
