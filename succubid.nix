{
  pkgs,
}:

pkgs.stdenv.mkDerivation (finalAttrs:
  let
    install-systemd = pkgs.writeShellApplication {
        name = "install-succubid";
        text = ''
          SERVICE_DIR="$HOME/.config/systemd/user"
          OVERRIDE_DIR="$SERVICE_DIR/succubid.service.d"

          mkdir -p "$SERVICE_DIR" "$OVERRIDE_DIR"

          cat > "$SERVICE_DIR/succubid.service" <<EOF
          [Unit]
          Description=Succubid
          After=default.target

          [Service]
          Type=simple
          ExecStart={{pkg}}
          Restart=on-failure
          RestartSec=5

          [Install]
          WantedBy=default.target
          EOF

          printf "Enter your Handy connection key: "
          read -r CONNECTION_KEY

          cat > "$OVERRIDE_DIR/10-env.conf" <<EOF
          [Service]
          Environment=SUCCUBID_HANDY_CONNECTION_KEY=$CONNECTION_KEY
          EOF

          systemctl --user daemon-reload
          systemctl --user enable --now succubid.service

          echo
          echo "Succubid has been installed as a user service."
        '';
      };
  in
{
  pname = "succubid";
  version = "0-unstable-2026-08-05";

  src = ./.;
  nativeBuildInputs = [
    pkgs.curl
    pkgs.makeWrapper
    pkgs.xxd
    pkgs.httplib
    pkgs.nlohmann_json
  ];
  configurePhase = "mkdir -p $out/bin/";
  installPhase = ''
    cp ./succubid $out/bin/unwrapped-succubid
    makeWrapper $out/bin/unwrapped-succubid $out/bin/succubid --add-flags "-g" --add-flags "-s" --add-flags "''${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/mpv.sock"
    makeWrapper ${pkgs.lib.getExe pkgs.mpv} $out/bin/mpv --add-flags "--input-ipc-server=''${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/mpv.sock"
    cat ${pkgs.lib.getExe install-systemd} | sed  "s,{{pkg}},$out/bin/succubid," > $out/bin/install-succubid; chmod +x $out/bin/install-succubid
  '';

  meta = {
    description = "A Unix Daemon for syncing NSFW videos with The Handy";
    homepage = "https://github.com/UnknownPleasuresDev/Succubid";
    license = pkgs.lib.licenses.agpl3Only;
    maintainers = [ "UnknownPleasuresDev" ];
    mainProgram = "succubid";
    platforms = pkgs.lib.platforms.all;
  };
})
