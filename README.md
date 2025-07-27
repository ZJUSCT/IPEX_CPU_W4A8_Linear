# IPEX_CPU_W4A8_Linear
AMX matrix multiplication implementation of a 4-bit quantified version that is better than libxsmm when the batch_size exceeds 16.

I rewrote the AMX operation part of the 4-bit quantization code in the woq linear section, and the model used for testing is DeepSeek-R1-Distill-Qwen-32B.

I conducted performance testing and found a 20% improvement compared to the original code. The result chart is as follows.

<img width="1691" height="1214" alt="903248cf3f9da088a69fdeb2cec7076b" src="https://github.com/user-attachments/assets/75100944-b241-402c-a2ae-15a6b0fa130e" />

In this repository, I only provided the location of the modified code. You just need to replace the original files and use the official script to compile it.
