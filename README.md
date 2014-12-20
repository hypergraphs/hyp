% hyp

`hyp` is a general toolkit for the representation,
manipulation, and optimization of weighted hypergraphs. Finite-state
machines are modeled as a special case.

The `hyp` toolkit provides data structures and algorithms to process
weighted directed hypergraphs.

Such hypergraphs are important in natural language processing and
machine learning. Their use arises naturally in parsing, syntax-based
machine translation and other tree-based models, as well as in logic
and weighted logic programming.

This library and the `Hyp*` command line tools provide data structures
for hypergraphs as well as many algorithms: `compose`, `project`,
`invert`, the shortest path algorithm, the inside and outside
algorithm, and more. In addition, it provides functionality to
optimize hypergraph feature weights from training data.

For documentation see
[`hyp-tutorial.pdf`](https://github.com/hypergraphs/hyp/raw/master/hyp-tutorial.pdf)
provided in this directory for your immediate gratification. (you can also build the pdf with
[TeX](https://www.tug.org/texlive/): `docs/hyp/hyp-tutorial.tex`)

Build the library by running [CMake](http://www.cmake.org/download/)
on the project sources in the `sdl` directory, after setting
environment variable `SDL_EXTERNALS_PATH` to an appropriate checkout
of the hypergraph library dependencies (which you can build yourself
from source if you prefer, but you will need to use or modify our
`FindXXXX.cmake` scripts).

For example, on windows: `set SDL_EXTERNALS_PATH=c:/src/sdl-externals/Windows` and then `cmake -DCMAKE_BUILD_TYPE=Release -G "Visual Studio 10 Win64" c:/src/hyp/sdl`

Or on linux:
```
export SDL_EXTERNALS_PATH=$HOME/src/sdl-externals/FC12;
git clone https://github.com/hypergraphs/hyp.git;
cd hyp;
mkdir build-hyp;
cd build-hyp;
cmake -DCMAKE_BUILD_TYPE=Release ../sdl && make -j4 VERBOSE=1`
```

Then, when you're ready to use or improve the source code for your own
projects, consider generating [Doxygen](http://www.doxygen.org/):
`doxygen doxy/doxy.conf`.

If you're willing to give us permission to use your source code, and
any patents required for it, we welcome pull requests. Keep your diffs
small, or talk to us before getting too crazy so we can save you some
wasted effort.