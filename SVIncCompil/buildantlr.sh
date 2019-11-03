#########################################################################
# Build Antlr4.72 for Java and C++ 
#########################################################################

mkdir -p ../antlr4
cd ../antlr4
wget https://github.com/antlr/antlr4/archive/4.7.2.zip
unzip 4.7.2.zip
cp -R ../third_party/antlr4/runtime/*              antlr4-4.7.2/runtime
cd antlr4-4.7.2
export MAVEN_OPTS="-Xmx1G"   
mvn clean                   
mvn -DskipTests install

# Optimized version
cd runtime/Cpp
rm -rf build run
mkdir build && mkdir run && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free"
make -j 4
DESTDIR=../../../runtime/Cpp/run make install

# Debug version:
cd ../../
cp -r Cpp Cpp-Debug
cd Cpp-Debug
rm -rf build run
mkdir build && mkdir run && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j 4
DESTDIR=../../../runtime/Cpp-Debug/run make install

# Memory Sanitizer version:
cd ../../
cp -r Cpp Cpp-AdvancedDebug
cd Cpp-AdvancedDebug
rm -rf build run
mkdir build && mkdir run && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-D_GLIBCXX_DEBUG=1 -fsanitize=address -fno-omit-frame-pointer"
make -j 4
DESTDIR=../../../runtime/Cpp-AdvancedDebug/run make install