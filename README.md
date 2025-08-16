# Overview
This project was commissioned by Dr. Johan Kwisthout from Radboud University. Dr. Kwisthout proposed the Most Frugal Explanation (MFE) as a way to speed up the computation of the mamximum a posteriori (MAP) problem. This problem is NP-Hard but the MFE attempts to reduce complexity by calculating the relevence of each of the variables and "ignoring" those variables that are irrelevant. When Dr. Kwisthout implemented MFE using the libDAI library no siginificant improvement was found and it was suspected this was because of how the MAP computation was being done (Kwisthout, 2022).  In essence the original MAP computation was using a junction tree (JT) algorithm to eliminate all the intermediate variables and then generating a joint table over the MAP variables. To find the MAP instantiation the joint table was searched to find the row with the highest probability. This approach worked but did not take advantage of the independence of MAP variables. 

Because the junction tree approach did not take advantage of the independence of the MAP variables, it was decided to implement the variable elimination algorithm in libDAI to compute MAP exactly. The algorithm was implemented and can compute the instantiations, however on some problem instances it takes significantly longer than the initial junction tree algorithm - see the test data in [final_test_runs.xlsx](final_test_runs.xlsx). So while the goal of implementing exact MAP was achieved, the goal of improving the speed was not. The rest of this document explains how to run variable elimination and also contains a long list of notes and to-do items for future work.  

# Running
## Building
Inside the LIBDAI MODIFIED directory, open a terminal and enter this command to build all of libDAI:

```
make
```

To build only what is necessary to run the variable elimination algorithm run the following:
```
make examples/example_map
```

## Running
To run a MAP query you can enter a command that follows this example:
```
examples/example_map -i ./factorgraphformats/hailfinder_fix.fg -o ./hailfinder_h5_map.txt -H 0,1,2,5,6 -E 43,44,45,46,47,48,49,50,51,52,53,54,55 -e 1,6,1,1,0,1,0,2,3,3,1,1,4 -M 
```

- `-i` specifies the bayesian network input file. Should be in factor graph (fg) file format.
- `-o` specifies where to store the result of the query. If the file does not exist yet the file will be created.
- `-H` specifies the hypothesis\query\map variables
- `-E` is the list of evidence (observed) variables
- `-e` is the list of values assigned to the evidence variables
- `-M` specifies to perform the MAP query on the network with the provided hypothesis variables and evidence

For more options, look at `example_map.cpp`.

# Variable Elimination To Do

1. It was noticed that the MAP instantiation produced by the variable elimination algorithm was not always the same as that produced by the junction tree algorithm. Queries on the `alarm_fix.f` specifically resulted in different instantiations. It is possible that during the maximisation step of the VE algorithm that two rows had the same maximum probability and thus an arbitrary choice was made. It is possible there are multiple valid MAP instantiations but this should be investigated further as it may also indicate a bug in one of the implementations.

2. In addition to the actual MAP instantiation, the probability associated with the maximum instantiation usually did not match between the junction tree and the variable elimination algorithms. This is because throughout the JT algorithm, the intermediate factors are constantly normalised but in the VE algorithm the factors are not normalised. Normalisation is not strictly necessary for computing the MAP instantiation because we are just searching for the maximum instantiation, but the actual probability of that instantiation is less important. However, Koller and Friedman in *Probabilistic Graphical Models: Principles and Techniques* (D. Koller & Friedman, 2009) describe some advantages of using normalisation to avoid possible underflow errors. See *Box 10.A - Skill: Efficient Implementation of Factor Manipulation Algorithms* in the book.  

3. When applying evidence at the beginning of the VE algorithm, a new method called `clampReduce()` inside of `factorgraph.cpp` was used. The original `clamp()` method in the same file simply set those factor rows that contradicted the evidence to have a probability of 0. While this works I thought it was inefficient because the entire row could be removed. Additionally, the evidence variable could be removed entirely from the factor, resulting in smaller `Varsets` and also smaller factor sizes when the rows are removed. The `clampReduce()` method seems to work well when used with the VE algorithm but produces incorrect instantiations when used with the JT algorithm which is not expected. Should be investigated.

4. It is possible that one of the reasons for the poor performance of the VE algorithm is due to the way the instantiation data is tracked. To track instantiation data I chose to implement extended factors as recommened in Darwiche (2009). These extended factors not only track the probability of each row in the factor but also track the maximised-out variable instantiations. While this works I suspect it is highly inefficient since all factors must carry around a whole new column of map data structures that are only populated with data at the very end of the algorithm when we are maxisiming out variables. For speed and storage efficiency it may be better to remove extended factors and instead just use some traceback algorithm. 

5. To investigate further why the VE and JT runtimes are so different on some of the networks, it may be worth experimenting with the VE algorithm and simply running the marginalisation process, generating the joint over the MAP variables, and then searching the joint for the MAP instantiation. So basically removing the maximnisation stept from VE and seeing if the runtimes more closely align after this. This might indicate where the extra time is coming from. 

6. I'm sure there are ways to optimise the VE algorithm to make it quicker. It should be combed through and checked to make sure no unncessary factor copies are being made and that everything is being passed by reference rather than by value. 

7. I was curious how the junction tree algorithm was doing so well on the MAP queries. The standard Juction Tree Message Passing algorithm is capable of calculating marginal probabilities for queries where all the query variables are contained within the same clique or cluster of the junction tree. But if the MAP query variables were contained in different clusters I couldn't figure out how it would calculate the marginal over these variables. I found a possible answer for how it is doing this in D. Koller and Friedman (2009) in section 10.3.3.2 *Queries Outside a Clique.*

# Notes on Other Approaches

## Junction Tree
The initial idea for the project was to implement the exact MAP on top of the junction tree algorithm. However it proved to be difficult to find resources that described such an implementation. Dawid (1992) describes a possible approach but implementing it would have taken significant time. Worth considering for future work. However, for a single query it was found that the theoretical time and space complexity are essentially identical for the variable elimination algorithm and the juctiont tree algorithm. It is possible that there are implementation details and large constants hidden in the big-O notation that result in variable elimination taking more time. However the main advantage of the junction tree algorithm is the fact it can store intermediate calculations for use on future queries - thus for multiple queries the junction tree algorithm is preferred to the Variable Elimination algorithm.

## Algorithm for Anytime Marginal MAP
Mauá and De Campos (2012) proposed an algorithm that makes a tradeoff between pareto points and pushing the maximisation sum to the end. It is not ideal to push the maximisations to the end as this results in a constrained elimination order with potentially very high treewidth. This paper is worth future investigation.


# References
1. Darwiche, A. (2009). Modeling and Reasoning with Bayesian Networks. Cambridge University Press.
2. Koller, D., & Friedman, N. (2009). Probabilistic graphical models: Principles and Techniques. MIT Press.
3. Kwisthout, J. (2022, September 19). Speeding up approximate MAP by applying domain knowledge about relevant variables. PMLR. https://proceedings.mlr.press/v186/kwisthout22a.html
4. Mauá, D. D., & De Campos, C. (2012). Anytime marginal maximum a posteriori inference. ResearchGate. https://www.researchgate.net/publication/279703286_Anytime_marginal_maximum_a_posteriori_inference
5. Dawid, A. P. (1992). Applications of a general propagation algorithm for probabilistic expert systems. Statistics and Computing, 2(1), 25–36. https://doi.org/10.1007/bf01890546


