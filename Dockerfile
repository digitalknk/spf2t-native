FROM docker.io/library/python:3.13-slim-bookworm@sha256:e32be0e55814a08977501ca863271d9bd02443710db8f2afaf8372e2c4508530 AS toolchain
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ cmake make pkg-config curl ca-certificates patchelf \
    libasound2-dev libpulse-dev libpipewire-0.3-dev libx11-dev libxext-dev libxrandr-dev \
    libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
    libwayland-dev libwayland-bin wayland-protocols libxkbcommon-dev \
    libdrm-dev libgbm-dev libegl1-mesa-dev libgl1-mesa-dev libudev-dev \
    && rm -rf /var/lib/apt/lists/*
RUN curl -fL https://www.libsdl.org/release/SDL3-3.4.16.tar.gz -o /tmp/SDL.tar.gz \
    && echo '7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68  /tmp/SDL.tar.gz' | sha256sum -c - \
    && mkdir /tmp/sdl && tar -xzf /tmp/SDL.tar.gz --strip-components=1 -C /tmp/sdl \
    && cmake -S /tmp/sdl -B /tmp/sdl-build -DCMAKE_BUILD_TYPE=Release \
       -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF \
       -DSDL_EXAMPLES=OFF -DSDL_WAYLAND_LIBDECOR=OFF \
    && cmake --build /tmp/sdl-build --parallel 2 \
    && cmake --install /tmp/sdl-build \
    && mkdir -p /usr/local/share/licenses/SDL3 \
    && cp /tmp/sdl/LICENSE.txt /usr/local/share/licenses/SDL3/LICENSE.txt \
    && ldconfig && rm -rf /tmp/sdl /tmp/sdl-build /tmp/SDL.tar.gz
# PipeWire client configuration loads these modules at runtime.
RUN apt-get update && apt-get install -y --no-install-recommends \
    libpipewire-0.3-modules libspa-0.2-modules pipewire-bin \
    && rm -rf /var/lib/apt/lists/*
ENV PYTHONDONTWRITEBYTECODE=1

FROM toolchain AS native-only
WORKDIR /opt/native
COPY . .
ENTRYPOINT ["python3", "/opt/native/tools/build.py"]
CMD ["--decomp", "/output/decomp", "--build-dir", "/tmp/native-build", "--output", "/output/spf2t-native"]

# Supply --build-context decomp=/path/to/local/spf2t-decomp for this target.
FROM native-only AS aio-local
COPY --from=decomp /include/puzzle_fighter/*.hpp /opt/decomp/include/puzzle_fighter/
COPY --from=decomp /tools/*.py /tools/*.cpp /opt/decomp/tools/
COPY --from=decomp /third_party/cps2crypt/*.cpp /third_party/cps2crypt/*.h /opt/decomp/third_party/cps2crypt/
COPY --from=decomp /third_party/*.txt /opt/decomp/third_party/
COPY --from=decomp /roms/spf2tu.json /opt/decomp/roms/spf2tu.json
COPY --from=decomp /LICENSE /LICENSE_SCOPE.md /THIRD_PARTY_NOTICES.md /UPSTREAM.md /source-manifest.json /README.md /DATA_FORMAT.md /CONTAINERS.md /VERSION /CMakeLists.txt /decompile.py /Dockerfile /.dockerignore /.gitignore /opt/decomp/
ENTRYPOINT ["python3", "/opt/native/tools/aio.py"]
CMD ["--decomp-project", "/opt/decomp", "--rom-dir", "/roms", "--output", "/output", "--build-dir", "/tmp/native-build"]

# Default AIO: include the latest decomp tooling project at image build time.
FROM native-only AS aio
RUN apt-get update && apt-get install -y --no-install-recommends git \
    && rm -rf /var/lib/apt/lists/*
ARG DECOMP_REPOSITORY=https://github.com/gemmy-turbo/spf2t-decomp.git
ARG DECOMP_REF=HEAD
RUN git init /opt/decomp \
    && git -C /opt/decomp remote add origin "$DECOMP_REPOSITORY" \
    && git -C /opt/decomp fetch --depth 1 origin "$DECOMP_REF" \
    && git -C /opt/decomp checkout --detach FETCH_HEAD \
    && git -C /opt/decomp rev-parse HEAD > /opt/decomp/image-source-revision.txt \
    && rm -rf /opt/decomp/.git
ENTRYPOINT ["python3", "/opt/native/tools/aio.py"]
CMD ["--decomp-project", "/opt/decomp", "--rom-dir", "/roms", "--output", "/output", "--build-dir", "/tmp/native-build"]
