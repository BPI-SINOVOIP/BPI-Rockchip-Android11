from autotest_lib.client.cros.faft import rpc_functions


class RPCProxy(object):
    """Proxy to the FAFT RPC server on DUT.

    This stub class (see PEP-484) tells IDEs about the categories and methods
    that are available on RPCProxy via __getattr__.
    """

    bios: rpc_functions.BiosServicer
    cgpt: rpc_functions.CgptServicer
    ec: rpc_functions.EcServicer
    kernel: rpc_functions.KernelServicer
    rootfs: rpc_functions.RootfsServicer
    rpc_settings: rpc_functions.RpcSettingsServicer
    system: rpc_functions.SystemServicer
    tpm: rpc_functions.TpmServicer
    updater: rpc_functions.UpdaterServicer
