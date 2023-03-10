# Runahead papers

Below is a list of papers related to runahead execution in chronological order and a short description of their key contributions.

* [2003] [Runahead Execution: An Alternative to Very Large Instruction Windows for Out-of-order Processors](https://people.inf.ethz.ch/omutlu/pub/mutlu_hpca03.pdf)
  * Traditional runahead/original paper
  * Also see [Mutlu's PhD defense](https://people.inf.ethz.ch/omutlu/pub/mutlu_phd_defense_talk.ppt)
* [2009] [MLP-Aware Runahead Threads in a Simultaneous Multithreading Processor](https://users.elis.ugent.be/~leeckhou/papers/hipeac09_kenzo.pdf)
  * Adjust entry condition into RE based on farthest MLP opportunity to reduce short, wasteful RE periods
* [2010] [Efficient Runahead Threads](https://people.inf.ethz.ch/omutlu/pub/efficient-rat_pact10.pdf)
  * Limit the length of RE periods to not run any longer than they are useful
* [2015] [Filtered Runahead Execution with a Runahead Buffer](https://dl.acm.org/doi/10.1145/2830772.2830812)
  * Remove instructions that do not lead to loads. There is no excuse not to do this
* [2016] [Continuous Runahead: Transparent Hardware Acceleration for Memory Intensive Workloads](https://people.inf.ethz.ch/omutlu/pub/continuous-runahead-engine_micro16.pdf)
  * Dedicated, simplified HW to generate LLLs
* [2020] [Precise Runahead Execution](https://users.elis.ugent.be/~leeckhou/papers/hpca2020.pdf)
  * Avoid the overhead of entering/exiting RE by using available resources (IQ, regs, etc.)
  * State-of-the-art
* [2021] [Branch Runahead: An Alternative to Branch Prediction for Impossible to Predict Branches](https://dl.acm.org/doi/10.1145/3466752.3480053)
  * Dedicated hardware to runahead and compute branch results and communicate them back to the CPU
* [2021] [Vector Runahead](https://users.elis.ugent.be/~leeckhou/papers/isca2021.pdf)
  * Vectorize loops to compute load dependencies and issue loads en masse
  * State-of-the-art with vector HW
* [2022] [Reliability-Aware Runahead](https://users.elis.ugent.be/~leeckhou/papers/hpca2022-RAR.pdf)
  * Do PRE, but flush on exit to remove state vulnerable to soft errors
  * Comparable perf to PRE with far fewer soft errors
