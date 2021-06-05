# sudo docker build --no-cache -t thierrytct/mart:llvm-9.0.0 . --build-arg llvm_version=9.0.0 && sudo docker push thierrytct/mart:llvm-9.0.0

#ARG llvm_version=3.8.1
ARG llvm_version=3.4.2
#ARG llvm_version=9.0.0

FROM thierrytct/llvm:$llvm_version
ARG llvm_version=3.4.2
#ARG llvm_version=9.0.0

# set this to enable mutant selection (with features axtraction)
# This relies on dg, so careful with newer LLVM versions, need to
# update compatibility with dg
ARG mutant_selection_on

ARG mart_location=/home/MART

RUN mkdir -p $mart_location/build $mart_location/src

#git clone https://github.com/thierry-tct/mart.git /tmp/mart/src
COPY . $mart_location/src

# fdupes needed for post compilation TCE. XXX 'make gitversion' is needed for dg
# libtinfo-dev is needed because of linking error with llvm-9 (problem -ltinfo)
# zlib1g-dev is needed because of linking error with llvm-9 ubuntu (problem -lz)
RUN apt-get -y install fdupes libtinfo-dev zlib1g-dev \
 && mkdir -p $mart_location/build && cd $mart_location/build \
 && if [ "$mutant_selection_on" = "" ]; then extra=""; else extra="-DMART_MUTANT_SELECTION=on"; fi \
 && cmake $extra -DLLVM_DIR=/usr/local/share/llvm/cmake/ $mart_location/src \
 && make CollectMutOpHeaders && { make gitversion || echo "No gitversion need"; } && make
ENV PATH="$mart_location/build/tools:${PATH}"

COPY ./example $mart_location/example

CMD ["$mart_location/bash", "$mart_location/example/run_example.sh"]


# && sed -i'' "s|/home/LLVM/llvm-$llvm_version/build_cmake/bin|$(dirname $(which clang))|g; s|/home/LLVM/llvm-$llvm_version/src/cmake/modules|/usr/local/share/llvm/cmake/|g" /usr/local/share/llvm/cmake/LLVMConfig.cmake \
