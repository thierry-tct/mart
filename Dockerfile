##############################################################################
# When specifying the version, without custom llvm, note that the last part of the version will be removed when looking for the package
# That is: for version 3.4.2, the package llvm-3.4 will be installed. and for  7.1, llvm-7 will be installed
#
# Some build examples:
# - LLVM 3.4.2
#   $ sudo docker build --no-cache -t thierrytct/mart:llvm-3.4.2 . --build-arg llvm_version=3.4.2 --build-arg base_image=thierrytct/llvm:3.4.2 --build-arg using_custom_llvm_base_image=on
#   $ sudo docker push thierrytct/mart:llvm-3.4.2
# - LLVM 11
#   $ sudo docker build --no-cache -t thierrytct/mart:llvm-11 . --build-arg llvm_version=11 --build-arg mutant_selection_on=on
#   $ sudo docker push thierrytct/mart:llvm-11
# - LLVM latest
#   $ sudo docker build --no-cache -t thierrytct/mart:llvm-latest . --build-arg mutant_selection_on=on
#   $ sudo docker push thierrytct/mart:llvm-latest
#
# When using the latest version on LLVM (with version specified), do
#   $ sudo docker image tag thierrytct/mart:llvm-<latest-llvm-version> thierrytct/mart:latest 
#   $ sudo docker push thierrytct/mart:latest
#
##############################################################################

ARG ubuntu_version_name=focal 
ARG base_image=docker.io/ubuntu:${ubuntu_version_name}

FROM $base_image AS base

ARG ubuntu_version_name

# Tells whether the specified base image is a custom llvm built image.
# In that case, llvm will not be installed anymore
ARG using_custom_llvm_base_image
# The llvm_version to use, in case not using custom llvm base image.
# if not specified in that case, the latest LLVM version is used
ARG llvm_version
# build type. one of: Debug, Release, RelWithDebInfo and MinSizeRel
# default is RelWithDebInfo
ARG built_type=Debug
# set this to enable mutant selection (with features extraction)
# This relies on dg, so careful with newer LLVM versions, need to
# update compatibility with dg
ARG mutant_selection_on
ARG mart_location=/home/MART

RUN mkdir -p ${mart_location}/build $mart_location/src

#git clone https://github.com/thierry-tct/mart.git /tmp/mart/src
COPY . ${mart_location}/src

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get -y update; exit 0
# Install LLVM if not using custom llvm base image
RUN if [ "${using_custom_llvm_base_image}" = "" ]; then \
        apt-get -y install cmake g++ wget software-properties-common \
            && apt-get -y update \
            && apt-get install -y python3-pip python3-dev \
            && cd /usr/local/bin \
            && ln -s /usr/bin/python3 python \
            && cd - \
            && { test -f $(dirname $(which pip3))/pip || ln -s $(which pip3) $(dirname $(which pip3))/pip; } \
            && pip install wllvm \
        || exit 1; \
        llvm_version_suffix=""; \
        [ "${llvm_version}" != "" ] && llvm_version_suffix="-${llvm_version}"; \
        if ! apt-cache search llvm${llvm_version_suffix} | grep "llvm${llvm_version_suffix} " > /dev/null; then \
            wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - \
                && add-apt-repository "deb http://apt.llvm.org/${ubuntu_version_name}/ llvm-toolchain-${ubuntu_version_name}${llvm_version_suffix%.*} main"\
                && apt-get -y update || exit 1; \
        fi; \
        apt-get -y install llvm${llvm_version_suffix} clang${llvm_version_suffix} llvm${llvm_version_suffix}-dev \
        || exit 1; \
        [ "${llvm_version}" != "" ] && ${mart_location}/src/update-alternatives-llvm-clang.sh ${llvm_version} 100; \
    fi

# fdupes needed for post compilation TCE. XXX 'make gitversion' is needed for dg
# libtinfo-dev is needed because of linking error for some versions (problem -ltinfo)
# zlib1g-dev is needed because of linking error for some version with ubuntu (problem -lz)
RUN apt-get -y install fdupes libtinfo-dev zlib1g-dev \
 && mkdir -p ${mart_location}/build && cd ${mart_location}/build \
 && if [ "${mutant_selection_on}" = "" ]; then extra=""; else extra="-DMART_MUTANT_SELECTION=on"; fi \
 && if [ "${using_custom_llvm_base_image}" != "" ]; then extra+=" -DLLVM_DIR=/usr/local/share/llvm/cmake/"; fi \
 && cmake ${extra} -DCMAKE_BUILD_TYPE=${built_type} ${mart_location}/src \
 && { make gitversion || echo "No gitversion need"; } && make

ENV PATH="${mart_location}/build/tools:${PATH}"
ENV MART_BINARY_DIR="${mart_location}/build/tools"

COPY ./example ${mart_location}/example

CMD ["bash", "-c", "${MART_BINARY_DIR}/../../example/run_example.sh"]


# && sed -i'' "s|/home/LLVM/llvm-$llvm_version/build_cmake/bin|$(dirname $(which clang))|g; s|/home/LLVM/llvm-$llvm_version/src/cmake/modules|/usr/local/share/llvm/cmake/|g" /usr/local/share/llvm/cmake/LLVMConfig.cmake \
