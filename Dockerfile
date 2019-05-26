FROM gcc:4.9
RUN apt-get update; exit 0
RUN apt-get -y install cmake \
&& svn co http://llvm.org/svn/llvm-project/llvm/tags/RELEASE_381/final /tmp/llvm-3.8.1/src \
&& svn co http://llvm.org/svn/llvm-project/cfe/tags/RELEASE_381/final /tmp/llvm-3.8.1/src/tools/clang \
&& mkdir /tmp/llvm-3.8.1/build && cd /tmp/llvm-3.8.1/build \
&& cmake /tmp/llvm-3.8.1/src && make -j2 \
&& make install && cd - && rm -rf /tmp/llvm-3.8.1 \
&& pip install wllvm \
&& mkdir -p /tmp/mart/build /tmp/mart/src

ENV LLVM_COMPILER=clang

#git clone https://github.com/thierry-tct/mart.git /tmp/mart/src
COPY . /tmp/mart/src

RUN cd /bin/mart/build && cmake /tmp/mart/src \
 && make CollectMutOpHeaders && make
ENV PATH="/tmp/mart/build/tools:${PATH}"

CMD ["mart", "--help"]

