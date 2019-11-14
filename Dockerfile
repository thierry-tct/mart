ARG llvm_version=3.8.1
ARG mart_location=/home/MART

FROM thierrytct/llvm:$llvm_version
RUN mkdir -p $mart_location/build $mart_location/mart/src

#git clone https://github.com/thierry-tct/mart.git /tmp/mart/src
COPY . $mart_location/src

# fdupes needed for post compilation TCE
RUN apt-get -y install fdupes \
 && mkdir -p $mart_location/build && cd $mart_location/build \
 && cmake -DMART_MUTANT_SELECTION=on $mart_location/src \
 && make CollectMutOpHeaders && make 
ENV PATH="$mart_location/build/tools:${PATH}"

COPY ./example $mart_location/example

CMD ["$mart_location/bash", "$mart_location/example/run_example.sh"]

