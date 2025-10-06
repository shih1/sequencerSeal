#!/bin/bash

# Documentation for pkgbuild and productbuild: https://developer.apple.com/library/archive/documentation/DeveloperTools/Reference/DistributionDefinitionRef/Chapters/Distribution_XML_Ref.html

# Input Arguments
INDIR=$1      # Directory containing the built files
SOURCEDIR=$2  # Directory containing resources/scripts (if any)
TARGET_DIR=$3 # Directory to store the installer
VERSION=$4    # Version string (e.g., "1.0.0")

# Sample usage
# ./make_installer.sh /path/to/built/files /path/to/resources /path/to/target/installer 1.0.0
# ./make_installer.sh ./../BuiltPlugins ./Resources ./ 1.0.0

if [ -z "$VERSION" ]; then
    echo "Error: Version must be provided as the fourth argument."
    exit 1
fi

# Define Plugin Details
TMPDIR=$(mktemp -d)
VST_FILENAME="pixel.clip.vst3"
AU_FILENAME="pixel.clip.component"
LICENSE_FILENAME="pixelLicense.txt"
OUTPUT_BASE_FILENAME="pixel-clip-$VERSION"

# Function to Build a Package
build_flavor() {
    flavor=$1
    flavorprod=$2
    ident=$3
    loc=$4

    if [ ! -e "$INDIR/$flavorprod" ]; then
        echo "$flavor source file not found. Skipping..."
        return
    fi

    echo "Building $flavor package..."
    mkdir -p "$TMPDIR/$flavor"
    cp -R "$INDIR/$flavorprod" "$TMPDIR/$flavor/"

    pkgbuild \
        --identifier "$ident" \
        --version "$VERSION" \
        --install-location "$loc" \
        --root "$TMPDIR/$flavor" \
        "$TMPDIR/$flavor.pkg"

    echo "$flavor package built."
}

# Build Packages
build_flavor "VST" "$VST_FILENAME" "com.pixel.pixel-clip.vst" "/Library/Audio/Plug-Ins/VST"
build_flavor "AU" "$AU_FILENAME" "com.pixel.pixel-clip.au" "/Library/Audio/Plug-Ins/Components"
build_flavor "License" "$LICENSE_FILENAME" "com.pixel.pixel-clip.license" "/Users/Shared/pixel_dsp"

# Generate Distribution XML
DISTRIBUTION_XML="$TMPDIR/distribution.xml"
cat > "$DISTRIBUTION_XML" <<EOL
<?xml version="1.0"?>
<installer-gui-script minSpecVersion="1.0">
    <title>Pixel Clip Installer</title>
    <background file="./PixelLogo.png" alignment="bottomleft"/>
    <title>pixel.clip ${VERSION}</title>
    <license file="license.txt" />
    <readme file="Readme.rtf" />
    <choices-outline>
        <line choice="vst"/>
        <line choice="au"/>
        <line choice="license"/>
    </choices-outline>
    <choice id="vst" title="Pixel Clip VST">
        <pkg-ref id="com.pixel.pixel-clip.vst"/>
    </choice>
    <choice id="au" title="Pixel Clip AU">
        <pkg-ref id="com.pixel.pixel-clip.au"/>
    </choice>
        <choice id="license" title="Pixel Clip License">
        <pkg-ref id="com.pixel.pixel-clip.license"/>
    </choice>
    <pkg-ref id="com.pixel.pixel-clip.vst" version="$VERSION" installKBytes="0">#vst.pkg</pkg-ref>
    <pkg-ref id="com.pixel.pixel-clip.au" version="$VERSION" installKBytes="0">#au.pkg</pkg-ref>
    <pkg-ref id="com.pixel.pixel-clip.license" version="$VERSION" installKBytes="0">#license.pkg</pkg-ref>
</installer-gui-script>
EOL

# Build Final Installer
productbuild \
    --distribution "$DISTRIBUTION_XML" \
    --resources "$SOURCEDIR/resources" \
    --package-path "$TMPDIR" \
    "$TARGET_DIR/$OUTPUT_BASE_FILENAME.pkg"

# Cleanup
rm -rf "$TMPDIR"

echo "Installer built successfully: $TARGET_DIR/$OUTPUT_BASE_FILENAME.pkg"
