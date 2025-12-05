FROM espressif/idf:release-v5.0

ARG DEBIAN_FRONTEND=nointeractive

RUN apt-get update \
  && apt install -y -q \
  cmake \
  git \
  libglib2.0-0 \
  libnuma1 \
  libpixman-1-0 \
  usbutils \
  libusb-1.0-0 \
  libusb-1.0-0-dev

RUN /opt/esp/python_env/idf5.0_py3.8_env/bin/pip install --no-cache-dir idf-component-manager

ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8
ENV IDF_PYTHON_ENV_PATH=/opt/esp/python_env/idf5.0_py3.8_env

RUN echo $($IDF_PATH/tools/idf_tools.py export) >> $HOME/.bashrc

# Ensure we run as root
USER root

ENTRYPOINT [ "/opt/esp/entrypoint.sh" ]

CMD ["/bin/bash"]