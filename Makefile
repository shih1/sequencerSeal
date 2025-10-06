# === Config ===
PLUGIN_NAME = pixel.clip.v1

# Allow BUILD_TYPE to be overridden: make BUILD_TYPE=Debug
# BUILD_TYPE ?= Release
BUILD_TYPE ?= Debug
BUILD_DIR = build
ARTEFACT_DIR = $(BUILD_DIR)/pixel-clip_artefacts/$(BUILD_TYPE)
CMAKE_BUILD_TYPE = $(BUILD_TYPE)

APPLE_ID = christophershih2@gmail.com
APP_SPECIFIC_PASSWORD = znqd-pijw-brxi-tmrq
TEAM_ID = 8NV24GUWGA

DEVELOPER_ID = "Developer ID Application: Christopher Jadee Shih (8NV24GUWGA)"
DEVELOPER_ID_APP = "Developer ID Application: Christopher Jadee Shih (8NV24GUWGA)"

AU_PATH = pkg-root/Library/Audio/Plug-Ins/Components/pixel.clip.v1.component
VST3_PATH = pkg-root/Library/Audio/Plug-Ins/VST3/pixel.clip.v1.vst3
APP_PATH = pkg-root/Applications/pixel.clip.v1.app

ZIP_DIR = notarize-temp
INSTALLER_ID = Developer ID Installer: Christopher Jadee Shih (8NV24GUWGA)

IDENTIFIER = com.pixel.pixel-clip

VST3_BUNDLE = $(ARTEFACT_DIR)/VST3/$(PLUGIN_NAME).vst3
AU_BUNDLE = $(ARTEFACT_DIR)/AU/$(PLUGIN_NAME).component
STANDALONE_APP = $(ARTEFACT_DIR)/Standalone/$(PLUGIN_NAME).app

PKG_ROOT = pkg-root
PKG_PATH = dist/pixel.clip.v1.pkg
SIGNED_PKG = $(PKG_PATH)
UNSIGNED_PKG = dist/pixel.clip.v1-unsigned.pkg


# === Targets ===
all: build sign pkg

# Quick development build (no signing)
dev: build

build:
	@echo "=== Building $(PLUGIN_NAME) ($(BUILD_TYPE)) with CMake ==="
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)
	cmake --build $(BUILD_DIR) --config $(CMAKE_BUILD_TYPE)

sign:
	@echo "=== Code signing plugin formats ==="
	codesign --force --timestamp --options runtime --deep -s $(DEVELOPER_ID) "$(VST3_BUNDLE)"
	codesign --force --timestamp --options runtime --deep -s $(DEVELOPER_ID) "$(AU_BUNDLE)"
	codesign --force --timestamp --options runtime --deep -s $(DEVELOPER_ID) "$(STANDALONE_APP)"
	codesign --verify --verbose "$(VST3_BUNDLE)"
	codesign --verify --verbose "$(AU_BUNDLE)"
	codesign --verify --verbose "$(STANDALONE_APP)"

pkg: prepare-pkg-root sign-and-notarize-binaries
	@echo "=== Creating unsigned .pkg installer ==="
	mkdir -p dist
	pkgbuild \
		--root "$(PKG_ROOT)" \
		--identifier "$(IDENTIFIER)" \
		--install-location "/" \
		"$(UNSIGNED_PKG)"

	@echo "=== Signing .pkg installer ==="
	productsign \
		--sign '$(INSTALLER_ID)' \
		"$(UNSIGNED_PKG)" \
		"$(SIGNED_PKG)"

	$(MAKE) notarize

sign-and-notarize-binaries:
	@echo "=== Signing AU, VST3, and App bundles ==="
	codesign --force --deep --sign $(DEVELOPER_ID_APP) --options runtime --timestamp "$(AU_PATH)"
	codesign --force --deep --sign $(DEVELOPER_ID_APP) --options runtime --timestamp "$(VST3_PATH)"
	codesign --force --deep --sign $(DEVELOPER_ID_APP) --options runtime --timestamp "$(APP_PATH)"

	@echo "=== Creating temp zips for notarization ==="
	rm -rf $(ZIP_DIR) && mkdir -p $(ZIP_DIR)
	cd $(ZIP_DIR) && zip -r AU.zip "../$(AU_PATH)"
	cd $(ZIP_DIR) && zip -r VST3.zip "../$(VST3_PATH)"
	cd $(ZIP_DIR) && zip -r App.zip "../$(APP_PATH)"

	@echo "=== Submitting for notarization ==="
	xcrun notarytool submit $(ZIP_DIR)/AU.zip --apple-id "$(APPLE_ID)" --password "$(APP_SPECIFIC_PASSWORD)" --team-id "$(TEAM_ID)" --wait
	xcrun notarytool submit $(ZIP_DIR)/VST3.zip --apple-id "$(APPLE_ID)" --password "$(APP_SPECIFIC_PASSWORD)" --team-id "$(TEAM_ID)" --wait
	xcrun notarytool submit $(ZIP_DIR)/App.zip --apple-id "$(APPLE_ID)" --password "$(APP_SPECIFIC_PASSWORD)" --team-id "$(TEAM_ID)" --wait

	@echo "=== Stapling binaries ==="
	xcrun stapler staple "$(AU_PATH)"
	xcrun stapler staple "$(VST3_PATH)"
	xcrun stapler staple "$(APP_PATH)"

notarize:
	@echo "=== Submitting .pkg for notarization ==="
	xcrun notarytool submit "$(SIGNED_PKG)" \
		--apple-id "$(APPLE_ID)" \
		--password "$(APP_SPECIFIC_PASSWORD)" \
		--team-id "$(TEAM_ID)" \
		--wait
	@echo "=== Stapling notarization ticket ==="
	xcrun stapler staple "$(SIGNED_PKG)"
	@echo "=== Verifying stapled .pkg ==="
	spctl --assess --type install --verbose "$(SIGNED_PKG)"

prepare-pkg-root:
	@echo "=== Preparing pkg root structure ==="
	rm -rf "$(PKG_ROOT)"
	mkdir -p "$(PKG_ROOT)/Library/Audio/Plug-Ins/VST3"
	mkdir -p "$(PKG_ROOT)/Library/Audio/Plug-Ins/Components"
	mkdir -p "$(PKG_ROOT)/Applications"

	cp -R "$(VST3_BUNDLE)" "$(PKG_ROOT)/Library/Audio/Plug-Ins/VST3/"
	cp -R "$(AU_BUNDLE)" "$(PKG_ROOT)/Library/Audio/Plug-Ins/Components/"
	cp -R "$(STANDALONE_APP)" "$(PKG_ROOT)/Applications/"


verify:
	@echo "=== Verifying .pkg installer ==="
	spctl --assess --type install --verbose "$(SIGNED_PKG)"

	@echo "=== Verifying AU plugin ==="
	codesign --verify --verbose=4 "$(AU_PATH)"

	@echo "=== Verifying VST3 plugin ==="
	codesign --verify --verbose=4 "$(VST3_PATH)"

	@echo "=== Verifying Standalone App ==="
	codesign --verify --verbose=4 "$(APP_PATH)"
	spctl --assess --type exec --verbose "$(APP_PATH)"


clean:
	@echo "=== Cleaning build artifacts ==="
	rm -rf $(BUILD_DIR) $(PKG_PATH) $(PKG_ROOT) dist $(ZIP_DIR)


.PHONY: all build sign pkg clean prepare-pkg-root dev