# Setting up the sPHENIX environment

To install the package, you need to have the sPHENIX environment set up. You can do this by:

```bash
source /opt/sphenix/core/bin/sphenix_setup.sh -n new
```

which gets the latest software stack, built around 10:30am each morning. We store a weekly build under ``ana.xxx`` tags. You can find them all under 

> /cvmfs/sphenix.sdcc.bnl.gov/gcc-12.1.0/release/release_ana/

You also need a location to store compiled libraries and headers. Typically this area is stored under the ```$MYINSTALL``` environment variable but it could be anything. The needed lines to use local copies is

```bash
export SPHENIX=/sphenix/u/cdean/sPHENIX
export MYINSTALL=$SPHENIX/install
export LD_LIBRARY_PATH=$MYINSTALL/lib:$LD_LIBRARY_PATH
export ROOT_INCLUDE_PATH=$MYINSTALL/include:$ROOT_INCLUDE_PATH
source /opt/sphenix/core/bin/setup_local.sh $MYINSTALL
```

Well this is my set up. I also have some extra variables for the JSON reader which I installed with vcpkg
```bash
# For vcpkg installs
export LD_LIBRARY_PATH=/sphenix/user/cdean/software/vcpkg/installed/x64-linux/lib:$LD_LIBRARY_PATH
export ROOT_INCLUDE_PATH=/sphenix/user/cdean/software/vcpkg/installed/x64-linux/include:$ROOT_INCLUDE_PATH
```

I keep this all in ```.bash_profile```

I wrote some aliases and functions in bash to compile and delete local files with ease
```bash
alias buildThisProject="mkdir build && cd build && ../autogen.sh --prefix=$MYINSTALL && make && make install && cd ../"

alias cleanThisProject='rm -rf aclocal.m4 autom4te.cache/ config.* configure depcomp install-sh ltmain.sh Makefile.in missing build/'

function cleanThisProjectFully()
{
  startDir=$(pwd)

  includeFolder=$(grep AC_INIT configure.ac | cut -d "(" -f2 | cut -d"," -f1)
  libraryNames=($(grep "\.la" Makefile.am | awk '{print $1}' | grep lib | cut -d "." -f1 | tr ' ' '\n' | sort -u | tr '\n' ' '))

  cleanThisProject

  cd $MYINSTALL

  rm -rf include/$includeFolder

  cd lib

  for i in "${libraryNames[@]}"
    do
      rm ${i}.la
      rm ${i}.so*
    done

  cd $startDir
}
```

**Note: If you build a project for the first time, you should re-source the local setup command so it picks up the new libraries. This isn't needed if you had a version of the package built when you initially sourced the environment. The environement will pick up any changes to the libraries and headers**

Run the ```buildThisProject``` command in the directory with the Makefile.am file in it

Once the project is built, switch to the macro directory and execute the Fun4All macro like you would a normal root macro

If you need help, we have a doxygen compiled from the daily build
[sPHENIX doxygen](https://sphenix-collaboration.github.io/doxygen/)
and the [sPHENIX github is found here](https://github.com/sPHENIX-Collaboration)
