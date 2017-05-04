# Match Only operator classes

This directory contain:
    - The base abstract class implemented by each mutation operator matched but never replaced. Example: mutating mutating _any constant_ into its double, but we can't mutate something into _any constant_. 
    - The directory operator contains the mutation operators implementing this base class (Examples are the _code Types_, the whole statement).
