# Consistent hashing library
## Usage

## Internals
### Summary
The goal is to achieve consistent weighted balancing over some values.
### Inputs
One should provide values to balance (reals) and their assigned ids
### Consistency over ids: unweighted rings
Same reals can be provided with different ids on different balancer instances.
With this in mind, choosing some id from set of all ids is not consistent.

To mitigate that unweighted rings are used. A ring is divided into segments by
hashed real values. Corresponding real id is assigned to each segment. Choosing 
a position in ring gives us an id with consistent real corresponding to it.

Using unweighted ring for choosing id has a drawback: hash values for reals partition
the ring unevenly. This leads to ids chosen by uniform random positions having
nonuniform distribution.

To mitigate that more than one unweighted ring (each with different seed used to
calculate hashes from reals). Iterating over rings when choosing position 

### Consistency over weights

