# sudo docker build --no-cache -t thierrytct/mart:llvm-3.4.2 . && sudo docker push thierrytct/mart:llvm-3.4.2

#ARG llvm_version=3.8.1
ARG llvm_version=3.4.2

FROM thierrytct/llvm:$llvm_version
ARG llvm_version=3.4.2

ARG mart_location=/home/MART

RUN mkdir -p $mart_location/build $mart_location/mart/src

#git clone https://github.com/thierry-tct/mart.git /tmp/mart/src
COPY . $mart_location/src

# fdupes needed for post compilation TCE. XXX 'make gitversion' is needed for dg
RUN apt-get -y install fdupes \
 && mkdir -p $mart_location/build && cd $mart_location/build \
 && cmake -DMART_MUTANT_SELECTION=on -DLLVM_DIR=/usr/local/share/llvm/cmake/ $mart_location/src \
 && make CollectMutOpHeaders && { make gitversion || echo "No gitversion need"; } && make
ENV PATH="$mart_location/build/tools:${PATH}"

COPY ./example $mart_location/example

CMD ["$mart_location/bash", "$mart_location/example/run_example.sh"]


# && sed -i'' "s|/home/LLVM/llvm-$llvm_version/build_cmake/bin|$(dirname $(which clang))|g; s|/home/LLVM/llvm-$llvm_version/src/cmake/modules|/usr/local/share/llvm/cmake/|g" /usr/local/share/llvm/cmake/LLVMConfig.cmake \
