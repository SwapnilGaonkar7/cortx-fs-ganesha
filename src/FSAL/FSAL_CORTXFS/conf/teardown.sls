Stage - Reset NFS:
  cmd.run:
    - name: __slot__:salt:setup_conf.conf_cmd('/opt/seagate/cortx/nfs/conf/setup.yaml', 'nfs_server:reset')

Remove NFS packages:
  pkg.removed:
    - pkgs:
      - cortx-dsal
      - cortx-dsal-devel
      - cortx-fs
      - cortx-fs-devel
      - cortx-fs-ganesha
      - cortx-fs-ganesha-test
      - cortx-nsal
      - cortx-nsal-devel
      - cortx-utils
      - cortx-utils-devel
    - disableexcludes: main

Remove NFS Ganesha:
  pkg.removed:
    - name: nfs-ganesha

# Removing libblkid & krb5-libs removes systemd and
# other important system libraries, don't remove them.
      #- libblkid
      #- krb5-libs

Delete nfs checkpoint flag:
  file.absent:
    - name: /opt/seagate/cortx/provisioner/generated_configs/{{ grains['id'] }}.nfs
