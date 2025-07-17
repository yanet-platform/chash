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
#### Lookup markup
Lookup is divided into segments. For each segment we choose an id: we select a
random position in current unweighted ring, and use the id that corresponds to
that position.

First segment start is chosen at location 0 of the ring. Each next segment starts
at the middle of the largest existing segment from left to right. We remember the
order of segments assign to each real.

Since sequence in which we add each new segment is the same and the seed of
uniform random sequence is same this markup is consistent between instances.

Because of element of randomness to distributing segments some ids will have
more segments assigned to them and some less. Once in a while we pause to rebalance:
excess segments are distributed between lacking. This is done inconsistently,
but effect of this is miniscule and ignored.

#### Weights
During lookup markup we composed a sequence of segments for each real. 
