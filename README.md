# Collection of simple 3D application based on OpenGL

# Building (Windows/Linux)
## It requires:
1. C++17
2. collection of repositories that can be downloaded and installed using https://github.com/dormon/checkRepos
## Steps:
1. cd pathToSomewhere/
2. build prerequisite repository: https://github.com/dormon/checkRepos .
This will create install directory pathToSomewhere/install/linux2/ or pathToSomewhere/install/win32.  
This is how the directory will look like after this step:
```
pathToSomewhere/git/checkRepos
pathToSomewhere/git/3DApps
pathToSomewhere/git/Vars
pathToSomewhere/git/glm
pathToSomewhere/git/...
pathToSomewhere/install/linux2/lib
pathToSomewhere/install/linux2/include
pathToSomewhere/install/linux2/...
...
```
3. $ cd git/3DApps
4. $ mkdir build
5. $ cd build
6. $ cmake -H.. -B. -DCMAKE_INSTALL_PREFIX=pathToSomewhere/install/linux2/ -DCMAKE_BUILD_TYPE=DEBUG
Setting CMAKE_INSTALL_PREFIX will make your life easier because it will set all cmake directories for all dependencies.
7. $ make
