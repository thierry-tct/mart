FROM thierrytct/llvm:3.8.1
mkdir -p /tmp/mart/build /tmp/mart/src

#git clone https://github.com/thierry-tct/mart.git /tmp/mart/src
COPY . /tmp/mart/src

RUN cd /bin/mart/build && cmake /tmp/mart/src \
 && make CollectMutOpHeaders && make
ENV PATH="/tmp/mart/build/tools:${PATH}"

COPY ./example /tmp

CMD ["/bin/bash", "/tmp/example/run_example.sh"]

