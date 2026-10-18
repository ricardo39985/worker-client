# Build from the attached FFmpeg source

This reconstructs the build procedure from the exact BtbN recipes and embedded
configure flags. It has not been run here. See NOTICE.md for source scope and
reproducibility limits. Do this only in a separate Linux build workspace with
adequate disk/RAM, Bash, tar, Docker with buildx, Git and Internet access already
available. No Windows worker, production system or host-wide tool installation
is involved. Review the scripts before running them.

1. Download `ffmpeg-source-and-notices.tar.gz` and `SHA256SUMS` from the same
   release as the binary. Verify the bundle against the published checksum:

   ```sh
   sha256sum --check --ignore-missing SHA256SUMS
   tar -xzf ffmpeg-source-and-notices.tar.gz
   cd ffmpeg-source-and-notices
   sha256sum --check SOURCE-SHA256SUMS
   tar -xzf FFmpeg-46d8f462eeb87ee1f704d8c44a0ee24fca471ad1.tar.gz
   tar -xzf FFmpeg-Builds-e88e49f624457c455700b058f0a84ca87d499cc2.tar.gz
   ```

2. Build the dependency image from the attached recipe revision. Do not check
   out today's `master` or use an existing floating `latest` image as evidence
   of the historical environment. `makeimage.sh` builds the base/toolchain and
   runs the pinned dependency-stage recipes, applying their included patches.
   Those recipes fetch dependency sources/submodules from the network; inspect
   `scripts.d/`, `images/`, `patches/` and `dependency-source-references.json`.
   Upstream toolchain/base package inputs are not completely pinned. Preserve
   downloaded dependency source, license notices, build logs and resulting image
   digests if preparing a further redistribution.

   ```sh
   source_dir="$PWD/FFmpeg-46d8f462eeb87ee1f704d8c44a0ee24fca471ad1"
   cd FFmpeg-Builds-e88e49f624457c455700b058f0a84ca87d499cc2
   unset GITHUB_REPOSITORY REGISTRY_OVERRIDE QUICKBUILD
   ./makeimage.sh win64 gpl 9.0
   ```

3. Compile the attached source instead of following the moving `release/9.0`
   branch used by the original `build.sh`. The command below follows that
   script's configure/make/install steps. It copies the source into a writable
   build volume and adds only a `VERSION` metadata file to preserve the known
   Git description absent from a GitHub tarball. It fixes the extra-version
   date to the observed original value. No FFmpeg source patch is applied by
   these commands. The BtbN dependency patches remain part of image creation.

   ```sh
   mkdir -p rebuild-output
   docker run --rm --user "$(id -u):$(id -g)" \
     -v "$source_dir":/source:ro \
     -v "$PWD/rebuild-output":/ffbuild \
     ghcr.io/btbn/ffmpeg-builds/win64-gpl-9.0:latest \
     bash -euc '
       cp -a /source /ffbuild/ffmpeg
       cd /ffbuild/ffmpeg
       printf "%s\n" n9.0.2-22-g46d8f462ee > VERSION
       ./configure --prefix=/ffbuild/prefix --pkg-config-flags=--static \
         $FFBUILD_TARGET_FLAGS $FF_CONFIGURE \
         --extra-cflags="$FF_CFLAGS" --extra-cxxflags="$FF_CXXFLAGS" \
         --extra-libs="$FF_LIBS" --extra-ldflags="$FF_LDFLAGS" \
         --extra-ldexeflags="$FF_LDEXEFLAGS" \
         --cc="$CC" --cxx="$CXX" --ar="$AR" --ranlib="$RANLIB" --nm="$NM" \
         --extra-version=20261001
       make -j2 V=1
       make install install-doc
     '
   ```

   The image named in step 3 is the locally built output of step 2. Keep it
   locally and record its digest; do not pull a replacement between the steps.
   The resulting executable is `rebuild-output/prefix/bin/ffmpeg.exe`. Compare
   its configure settings with the attached `configuration.txt`; compiler,
   dependency, timestamp or metadata differences can change its checksum.

4. Hash the rebuilt executable and test it separately on a supported, authorized
   Windows test system before use. A rebuild is a new artifact until reviewed;
   never overwrite the retained approved binary or change the worker's lock
   just because a local rebuild succeeds. No native rebuild/test is part of
   this publication operation.
