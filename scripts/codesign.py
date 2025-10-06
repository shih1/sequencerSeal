import os
import subprocess
import yaml

def run_command(command):
    print(f"Running command: {command}")
    try:
        subprocess.run(command, check=True, shell=True)
    except subprocess.CalledProcessError as e:
        print(f"Error executing command: {command}\n{e}")
        exit(1)

def load_config(config_path):
    try:
        with open(config_path, 'r') as file:
            return yaml.safe_load(file)
    except FileNotFoundError:
        print(f"Configuration file not found: {config_path}")
        exit(1)
    except yaml.YAMLError as e:
        print(f"Error parsing YAML configuration: {e}")
        exit(1)

def setup_credentials(config):
    commands = [
        f"""xcrun notarytool store-credentials "PixelAppCredentials" \
        --apple-id "{config['apple_id']}" \
        --team-id "{config['team_id']}" \
        --password "{config['app_specific_password']}" """,

            f"""xcrun notarytool store-credentials "PixelInstallerCredentials" \
        --apple-id "{config['apple_id']}" \
        --team-id "{config['team_id']}" \
        --password "{config['app_specific_password']}" """
        ]
    for command in commands:
        run_command(command)

def sign_plugin(file_path, developer_id):
    command = f"codesign --deep --force --verify --sign \"{developer_id}\" {file_path}"
    run_command(command)

def zip_plugins(output_path, *files):
    files_str = ' '.join(files)
    command = f"zip -r {output_path} {files_str}"
    run_command(command)


def unzip_plugins(output_path, *files):
    files_str = ' '.join(files)
    command = f"unzip {output_path} -d ./signed/"
    run_command(command)

def notarize(file_path, credentials_profile):
    command = f"xcrun notarytool submit \"{file_path}\" --keychain-profile \"{credentials_profile}\" --wait"
    run_command(command)

def staple(file_path):
    # Directly append '/signed' to the file path
    signed_path = f"./signed/{file_path}"
    command = f"xcrun stapler staple \"{signed_path}\""
    run_command(command)

def validate(file_path):
    # Directly append '/signed' to the file path
    signed_path = f"./signed/{file_path}"
    command = f"xcrun stapler validate \"{signed_path}\""
    run_command(command)

def sign_installer(input_pkg, output_pkg, developer_id):
    command = f"productsign --sign \"{developer_id}\" \"{input_pkg}\" \"{output_pkg}\""
    run_command(command)

def main():
    import argparse
    parser = argparse.ArgumentParser(description="Notarize and sign plugins or installers.")
    parser.add_argument('config', help="Path to notarize_config.yaml")
    parser.add_argument('--type', choices=['app', 'installer', 'credentials'], required=True, help="Specify whether to notarize an application or an installer")
    parser.add_argument('--help-info', action='store_true', help="Display script instructions")

    args = parser.parse_args()

    if args.help_info:
        print(__doc__)
        return

    config = load_config(args.config)
    # print(config)
    developer_id_app = config['developer_id_application']
    developer_id_installer = config['developer_id_installer']

    # Step 0: Set up keychain with credentials
    if args.type == 'credentials':
        setup_credentials(config)

    if args.type == 'app':
        # Step 1: Sign Plugins
        sign_plugin(config['path_to_unsigned_vst'], developer_id_app)
        sign_plugin(config['path_to_unsigned_au'], developer_id_app)

        # Step 2: Zip signed plugins
        zip_plugins(config['output_zip_path'], config['path_to_unsigned_vst'], config['path_to_unsigned_au'])

        # Step 3: Notarize the zipped plugins
        notarize(config['output_zip_path'], "PixelAppCredentials")


        # Step 2.5: Unzip the plugins
        unzip_plugins(config['output_zip_path'])

        # Step 4: Staple the notarized file
        staple(config['path_to_unsigned_vst'])
        staple(config['path_to_unsigned_au'])

        validate(config['path_to_unsigned_vst'])
        validate(config['path_to_unsigned_au'])

    elif args.type == 'installer':
        # Step 1: Sign the installer
        sign_installer(config['input_pkg'], config['output_signed_pkg'], developer_id_installer)

        # Step 2: Notarize the signed installer
        notarize(config['output_signed_pkg'], "PixelInstallerCredentials")

        # Step 3: Staple and validate the signed installer
        staple(config['output_signed_pkg'])
        validate(config['output_signed_pkg'])

if __name__ == "__main__":
    main()