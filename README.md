# IPEX_CPU_W4A8_Linear
AMX matrix multiplication implementation of a 4-bit quantified version that is better than libxsmm when the batch_size exceeds 16.

I rewrote the AMX operation part of the 4-bit quantization code in the woq linear section, and the model used for testing is DeepSeek-R1-Distill-Qwen-32B.

I conducted performance testing and found a 20% improvement compared to the original code. The result chart is as follows.

<img width="1929" height="1452" alt="image" src="https://github.com/user-attachments/assets/54288ec5-f1c0-4206-aa36-20d0bb7c1c8b" />

In this repository, I only provided the location of the modified code. You just need to replace the original files and use the official script to compile it.
