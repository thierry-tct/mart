FROM thierrytct/llvm:3.8.1
#RUN mkdir -p /tmp/mart/build /tmp/mart/src

#git clone https://github.com/thierry-tct/mart.git /tmp/mart/src
COPY . /tmp/mart/src

# fdupes needed for post compilation TCE
RUN apt-get -y install fdupes \
 && mkdir -p /tmp/mart/build && cd /tmp/mart/build \
 && cmake -DMART_MUTANT_SELECTION=on /tmp/mart/src \
 && make CollectMutOpHeaders && make 
ENV PATH="/tmp/mart/build/tools:${PATH}"

COPY ./example /tmp/example

CMD ["/bin/bash", "/tmp/example/run_example.sh"]

