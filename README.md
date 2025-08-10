Wordfish 1.0 is a free and strong UCI chess engine derived from Glaurung 2.1 that analyzes chess positions and computes the optimal moves.

Wordfish does not include a graphical user interface (GUI) that is required to display a chessboard and to make it easy to input moves. These GUIs are developed independently from Wordfish and are available online. Read the documentation for your GUI of choice for information about how to use Wordfish with it.

See also the Wordfish documentation for further usage help.

Files
This distribution of Wordfish consists of the following files:

README.md, the file you are currently reading.

Copying.txt, a text file containing the GNU General Public License version 3.

AUTHORS, a text file with the list of authors for the project.

src, a subdirectory containing the full source code, including a Makefile that can be used to compile Wordfish on Unix-like systems.

a file with the .nnue extension, storing the neural network for the NNUE evaluation. Binary distributions will have this file embedded.

Contributing
See Contributing Guide.

Donating hardware
Improving Wordfish requires a massive amount of testing. You can donate your hardware resources by installing the Fishtest Worker and viewing the current tests on Fishtest.

Improving the code
In the chessprogramming wiki, many techniques used in Wordfish are explained with a lot of background information. The section on Wordfish describes many features and techniques used by Wordfish. However, it is generic rather than focused on Wordfish's precise implementation.

The engine testing is done on Fishtest. If you want to help improve Wordfish, please read this guideline first, where the basics of Wordfish development are explained.

Discussions about Wordfish take place these days mainly in the Wordfish Discord server. This is also the best place to ask questions about the codebase and how to improve it.

Compiling Wordfish
Wordfish has support for 32 or 64-bit CPUs, certain hardware instructions, big-endian machines such as Power PC, and other platforms.

On Unix-like systems, it should be easy to compile Wordfish directly from the source code with the included Makefile in the folder src. In general, it is recommended to run make help to see a list of make targets with corresponding descriptions. An example suitable for most Intel and AMD chips:

cd src
make -j profile-build
Detailed compilation instructions for all platforms can be found in our documentation. Our wiki also has information about the UCI commands supported by Wordfish.

Terms of use
Wordfish is free and distributed under the GNU General Public License version 3 (GPL v3). Essentially, this means you are free to do almost exactly what you want with the program, including distributing it among your friends, making it available for download from your website, selling it (either by itself or as part of some bigger software package), or using it as the starting point for a software project of your own.

The only real limitation is that whenever you distribute Wordfish in some way, you MUST always include the license and the full source code (or a pointer to where the source code can be found) to generate the exact binary you are distributing. If you make any changes to the source code, these changes must also be made available under GPL v3.

Acknowledgements
Wordfish uses neural networks trained on data provided by the Leela Chess Zero project, which is made available under the Open Database License (ODbL).