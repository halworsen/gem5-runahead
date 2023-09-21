# Developing on SLURM clusters

Running this on Idun or some other cluster with SLURM? This is the (extremely informal) instruction
manual for developing the project in such a hostile environment.

## Making a module list (Idun specific)

You're going to need build dependencies to work with gem5. On Idun, software is managed as modules.
This makes the "building gem5" guide in the gem5 docs completely worthless because we can't `sudo apt`
our way to happiness. Instead, I've used the following collection of modules while developing for gem5:

```
[markuswh@idun-login1 gem5-runahead]$ module describe gem5
Collection "gem5" contains: 
   1) GCCcore/11.3.0                  4) intel-compilers/2022.1.0         7) impi/2021.6.0-intel-compilers-2022.1.0    10) HDF5/1.12.2-iimpi-2022a           13) ncurses/6.3-GCCcore-11.3.0          16) SQLite/3.38.3-GCCcore-11.3.0    19) libffi/3.4.2-GCCcore-11.3.0
   2) zlib/1.2.12-GCCcore-11.3.0      5) numactl/2.0.14-GCCcore-11.3.0    8) iimpi/2022a                               11) protobuf/3.19.4-GCCcore-11.3.0    14) libreadline/8.1.2-GCCcore-11.3.0    17) XZ/5.2.5-GCCcore-11.3.0         20) OpenSSL/1.1
   3) binutils/2.38-GCCcore-11.3.0    6) UCX/1.12.1-GCCcore-11.3.0        9) Szip/2.1.1-GCCcore-11.3.0                 12) bzip2/1.0.8-GCCcore-11.3.0        15) Tcl/8.6.12-GCCcore-11.3.0           18) GMP/6.2.1-GCCcore-11.3.0        21) Python/3.10.4-GCCcore-11.3.0
```

You probably want to save this collection of modules. So `module install` each of these, then *save*
the module list by running `module save gem5`. You can then restore all of the installed modules with
`module restore gem5`. So when you `make gem5` and get errors about missing stuff (typically
`/usr/bin/env: ‘python’: No such file or directory` on `make`), you just `module restore gem5`
and re-run `make gem5`. Should work then.

## Setting up debugging with VSCode

Runahead is cool!!! debugging gem5 sucks......

Working on gem5 without a debugger is painful. I did it for a whole semester and deeply regretted it.
Going by debug logs almost kind of works, but is a painful and extremely stressful,
often hours-long-per-bug experience. *Do not repeat my mistake.*

Making debugging work on Idun is a special flair of annoying because VSCode really hates dynamic inputs
for its launch.json configuration that specifies how to run debuggers. This is a problem, because
supercomputer clusters will typically run your jobs on completely random worker nodes when you submit
your jobs. We need to know which node gem5 is running on to connect to it with gdb.

### 1. Build GDB from source

Idun doesn't have gdb for some reason, so you should build it from source ASAP.
Go to the [home page for the GNU debugger (gdb)](https://www.sourceware.org/gdb/) and click
[download](https://www.sourceware.org/gdb/download/) to find official downloads for GDB.

I've used GDB 13.2 for debugging gem5. It worked fine for the specific version of gem5 that's pinned
as a git submodule. So that's what I'm using for this manual.

#### Quick setup (gdb with python support)

You want to build with python as it gives pretty printing for STL containers like lists, vectors, etc.

```bash
# use the gem5 module list you set up earlier
module restore gem5

# download, build and install gdb 13.2 from source
cd $HOME
curl -lo gdb-13.2.tar.gz https://sourceware.org/pub/gdb/releases/gdb-13.2.tar.gz
tar -xvzf gdb-13.2.tar.gz

cd gdb-13.2
export PYPATH=$(which python)
# configure the build to set the RPATH to ~/lib, allowing gdb search ~/lib for shared libraries at runtime
LDFLAGS="-Wl,-rpath,$HOME/lib" ./configure --prefix=$HOME --with-python=$PYPATH
# build gdb
make
# install gdb
make install

# Symlink dynamic libraries used by gdb into ~/lib
# This is only to make gdb work within VSCode since it doesn't let you set LD_LIBRARY_PATH before
# running a debug session. If using gdb from the shell, you can `module restore gem5` and run gdb fine.
cd $HOME/lib
ln -s /cluster/apps/eb/software/GCCcore/11.3.0/lib64/libstdc++.so.6 libstdc++.so.6
ln -s /cluster/apps/eb/software/ncurses/6.3-GCCcore-11.3.0/lib/libncursesw.so.6 libncursesw.so.6

```

Then make sure gdb is installed correctly and works.

```bash
[markuswh@idun-login1 gdb-13.2]$ ~/gdb-13.2/gdb/gdb
GNU gdb (GDB) 13.2
Copyright (C) 2023 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
Type "show copying" and "show warranty" for details.
This GDB was configured as "x86_64-pc-linux-gnu".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<https://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
    <http://www.gnu.org/software/gdb/documentation/>.

For help, type "help".
Type "apropos word" to search for commands related to "word".
(gdb)
```

### 2. Setup SSH access to the worker nodes

I'm a Windows pleb and therefore cannot use SSH agent forwarding to automagically gain SSH access to
the worker nodes on Idun. To work around this I made a new SSH key on Idun which I used to access
the worker nodes:

```bash
cd ~/.ssh
ssh-keygen -t rsa -b 4096
# dont make a passphrase :)
# append the public key to the authorized keys file so we can use it to connect to cluster machines
cat id_rsa.pub >> authorized_keys
```

Then put the following into `~/.ssh/config` so SSH knows to use the keyfile when connecting to
the worker nodes:

```plaintext
Host idun-0* idun-9*
    User <yourusername>
    IdentityFile ~/.ssh/id_rsa
```

### 3. Install the required VSCode extensions

You need a VSCode extension called
[Tasks Shell Input](https://marketplace.visualstudio.com/items?itemName=augustocdias.tasks-shell-input).

It's used to start gem5 under gdbserver and to find out which node it's running on so the debug task works.

### 4. magic

you should now be able to click the funny play button with a little bug and press the green play button
that says "Debug gem5".

that makes VSCode run a script which submits a SLURM job that starts gem5 under gdbserver. It then
surgically extracts the worker node the job is running on and the port that gdbserver uses (it's
actually hardcoded but whatever). then it starts up gdb and attaches to the worker node at the
given port as a remote target.

you have to wait (be patient SLURM may be slow), the debugger should start eventually (it's magic :o)

### 5. Didn't work?

Make sure you've built the debug variant of gem5. Run `make gem5-debug`.

Still no? I strongly suggest you invest a couple of days to find out how to make the debugger work.
If you feel like my excellent debugging setup is too cryptic or difficult to make work,
throw out all my hard work and **find out how to make it work.** Commandline gdb is good,
interactive VSCode debugger is better.

Going by debug logs almost kind of works, but is a painful and extremely stressful,
often hours-long-per-bug experience. **Do not repeat my mistake.**
