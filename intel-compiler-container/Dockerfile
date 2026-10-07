FROM gcc:latest
COPY ./src /src
WORKDIR /src
SHELL ["/bin/bash", "-c"]
RUN pwd && \
ls && \
if ! [[ -f intel-oneapi-base-toolkit-2025.3.2.21_offline.sh ]]; \
  then wget -q https://registrationcenter-download.intel.com/akdlm/IRC_NAS/99f4837a-25b7-425d-a897-60af022676ea/intel-oneapi-base-toolkit-2025.3.2.21_offline.sh; fi && \
sh ./intel-oneapi-base-toolkit-2025.3.2.21_offline.sh -a --silent --cli --eula accept &&\
rm ./intel-oneapi-base-toolkit-2025.3.2.21_offline.sh 

# These variables were extracted by running:
# . /opt/intel/oneapi/2025.3/oneapi-vars.sh
# env
ENV TBBROOT="/opt/intel/oneapi/2025.3"
ENV ONEAPI_ROOT="/opt/intel/oneapi/2025.3"
ENV PKG_CONFIG_PATH="/opt/intel/oneapi/vtune/2025.10/include/pkgconfig/lib64:/opt/intel/oneapi/advisor/2025.5/include/pkgconfig/lib64:/opt/intel/oneapi/2025.3/lib/pkgconfig"
ENV ADVISOR_2025_DIR="/opt/intel/oneapi/advisor/2025.5"
ENV GPG_KEYS="B215C1633BCA0477615F1B35A5B3A004745C015A 	B3C42148A44E6983B3E4CC0793FA9B1AB75C61B8 	90AA470469D3965A87A5DCB494D03953902C9419 	80F98B2E0DAB6C8281BDF541A7C8C3B2F71EDF1C 	7F74F97C103468EE5D750B583AB00996FC26A641 	33C235A34C46AA3FFB293709A328C3A2C3C45C06 	D3A93CAD751C2AF4F8C7AD516C35B99309B5FA62"
ENV CCL_ROOT="/opt/intel/oneapi/2025.3"
ENV I_MPI_ROOT="/opt/intel/oneapi/2025.3"
ENV GCC_VERSION="15.2.0"
ENV FI_PROVIDER_PATH="/opt/intel/oneapi/2025.3/opt/mpi/libfabric/lib/prov:/usr/lib64/libfabric"
ENV DNNLROOT="/opt/intel/oneapi/2025.3"
ENV DIAGUTIL_PATH="/opt/intel/oneapi/2025.3/etc/dpct/sys_check/sys_check.sh"
ENV CCL_CONFIGURATION="cpu_gpu_dpcpp"
ENV DPL_ROOT="/opt/intel/oneapi/2025.3"
ENV MANPATH="/opt/intel/oneapi/2025.3/share/man:"
ENV GDB_INFO="/opt/intel/oneapi/2025.3/share/info/"
ENV CCL_CONFIGURATION_PATH=""
ENV DYLD_LIBRARY_PATH="/opt/intel/oneapi/2025.3/lib"
ENV SETVARS_COMPLETED="1"
ENV GCC_MIRRORS="https://ftpmirror.gnu.org/gcc 		https://mirrors.kernel.org/gnu/gcc 		https://bigsearcher.com/mirrors/gcc/releases 		http://www.netgull.com/gcc/releases 		https://ftpmirror.gnu.org/gcc 		https://sourceware.org/pub/gcc/releases 		ftp://ftp.gnu.org/gnu/gcc"
ENV APM="/opt/intel/oneapi/advisor/2025.5/perfmodels"
ENV CMAKE_PREFIX_PATH="/opt/intel/oneapi/2025.3"
ENV DPLROOT="/opt/intel/oneapi/2025.3"
ENV CMPLR_ROOT="/opt/intel/oneapi/2025.3"
ENV INFOPATH="/opt/intel/oneapi/2025.3/share/info"
ENV IPPROOT="/opt/intel/oneapi/2025.3"
ENV IPP_TARGET_ARCH="intel64"
ENV PYTHONPATH="/opt/intel/oneapi/advisor/2025.5/pythonapi"
ENV CPLUS_INCLUDE_PATH="/opt/intel/oneapi/2025.3/include:/opt/intel/oneapi/2025.3/include/dal:/opt/intel/oneapi/2025.3/include"
ENV DALROOT="/opt/intel/oneapi/2025.3"
ENV LIBRARY_PATH="/opt/intel/oneapi/2025.3/lib"
ENV IPPCRYPTOROOT="/opt/intel/oneapi/2025.3"
ENV IPPCP_TARGET_ARCH="intel64"
ENV SHLVL="1"
ENV OCL_ICD_FILENAMES="/opt/intel/oneapi/2025.3/lib/libintelocl.so"
ENV CLASSPATH="/opt/intel/oneapi/2025.3/share/java"
ENV LD_LIBRARY_PATH="/opt/intel/oneapi/2025.3/opt/mpi/libfabric/lib:/opt/intel/oneapi/2025.3/opt/compiler/lib:/opt/intel/oneapi/2025.3/lib"
ENV IPP_VERSION="2022.3.0"
ENV VTUNE_PROFILER_DIR="/opt/intel/oneapi/vtune/2025.10"
ENV MKLROOT="/opt/intel/oneapi/2025.3"
ENV INTEL_TARGET_ARCH="intel64"
ENV VTUNE_PROFILER_2025_DIR="/opt/intel/oneapi/vtune/2025.10"
ENV NLSPATH="/opt/intel/oneapi/2025.3/lib/compiler/locale/%l_%t/%N:/opt/intel/oneapi/2025.3/lib/locale/%l_%t/%N"
ENV PATH="/opt/intel/oneapi/vtune/2025.10/bin64:/opt/intel/oneapi/advisor/2025.5/bin64:/opt/intel/oneapi/2025.3/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
ENV IPPCRYPTOVERSION="2021.9"
ENV INTEL_PYTHONHOME="/opt/intel/oneapi/2025.3/opt/debugger"
ENV C_INCLUDE_PATH="/opt/intel/oneapi/2025.3/include"


CMD ["ls"]