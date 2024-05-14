#include "../nvme.h"
#include "./ftl.h"

static void bb_init_ctrl_str(FemuCtrl *n)
{
    static int fsid_vbb = 0;
    const char *vbbssd_mn = "FEMU BlackBox-SSD Controller";
    const char *vbbssd_sn = "vSSD";

    nvme_set_ctrl_name(n, vbbssd_mn, vbbssd_sn, &fsid_vbb);
}

/* bb <=> black-box */
static void bb_init(FemuCtrl *n, Error **errp)
{
    struct ssd *ssd = n->ssd = g_malloc0(sizeof(struct ssd));

    bb_init_ctrl_str(n);

    ssd->dataplane_started_ptr = &n->dataplane_started;
    ssd->ssdname = (char *)n->devname;
    femu_debug("Starting FEMU in Blackbox-SSD mode ...\n");
    ssd_init(n);
}

static void bb_flip(FemuCtrl *n, NvmeCmd *cmd)
{
    struct ssd *ssd = n->ssd;
    int64_t cdw10 = le64_to_cpu(cmd->cdw10);

    switch (cdw10) {
    case FEMU_ENABLE_GC_DELAY:
        // ssd->sp.enable_gc_delay = true;
        femu_log("%s,FEMU GC Delay Emulation [Enabled]!\n", n->devname);
        break;
    case FEMU_DISABLE_GC_DELAY:
        // ssd->sp.enable_gc_delay = false;
        femu_log("%s,FEMU GC Delay Emulation [Disabled]!\n", n->devname);
        break;
    case FEMU_ENABLE_DELAY_EMU:
        // ssd->sp.pg_rd_lat_QLC = NAND_READ_LATENCY_QLC;
        // ssd->sp.pg_wr_lat_QLC = NAND_PROG_LATENCY_QLC;
        // ssd->sp.blk_er_lat_QLC = NAND_ERASE_LATENCY_QLC;
        // ssd->sp.ch_xfer_lat = 0;

        // ssd->sp.pg_rd_lat_SLC = NAND_READ_LATENCY_SLC;
        // ssd->sp.pg_wr_lat_SLC = NAND_PROG_LATENCY_SLC;
        // ssd->sp.blk_er_lat_SLC = NAND_ERASE_LATENCY_SLC;
        femu_log("%s,FEMU Delay Emulation [Enabled]!\n", n->devname);
        break;
    case FEMU_DISABLE_DELAY_EMU:
        // ssd->sp.pg_rd_lat_QLC = 0;
        // ssd->sp.pg_wr_lat_QLC = 0;
        // ssd->sp.blk_er_lat_QLC = 0;
        // ssd->sp.ch_xfer_lat = 0;

        // ssd->sp.pg_rd_lat_SLC = 0;
        // ssd->sp.pg_wr_lat_SLC = 0;
        // ssd->sp.blk_er_lat_SLC = 0;
        femu_log("%s,FEMU Delay Emulation [Disabled]!\n", n->devname);
        break;
    case FEMU_RESET_ACCT:
        n->nr_tt_ios = 0;
        n->nr_tt_late_ios = 0;
        femu_log("%s,Reset tt_late_ios/tt_ios,%lu/%lu\n", n->devname,
                n->nr_tt_late_ios, n->nr_tt_ios);
        break;
    case FEMU_ENABLE_LOG:
        n->print_log = true;
        femu_log("%s,Log print [Enabled]!\n", n->devname);
        break;
    case FEMU_DISABLE_LOG:
        n->print_log = false;
        femu_log("%s,Log print [Disabled]!\n", n->devname);
        break;
    case FEMU_PAGES_WRITTEN_STATISTIC:
        ftl_log("statistic for pages written is %lu\n",ssd->pages_written);
        ftl_log("statistic for pages read is %lu\n",ssd->pages_read);
        ftl_log("statistic for lines gc is %lu\n",ssd->gc_lines);
        ftl_log("statistic for lines migrated is %lu\n",ssd->migrate_lines);
        ftl_log("statistic for pages written to qlc is %lu\n",ssd->pages_to_qlc);
        ftl_log("statistic for pages written to slc is %lu\n",ssd->pages_to_slc);
        ssd->pages_written = 0;
        ssd->pages_read = 0;
        ssd->migrate_lines = 0;
        ssd->gc_lines = 0;
        ssd->pages_to_slc = 0;
        ssd->pages_to_qlc = 0;
        break;
    default:
        printf("FEMU:%s,Not implemented flip cmd (%lu)\n", n->devname, cdw10);
    }
}

static uint16_t bb_nvme_rw(FemuCtrl *n, NvmeNamespace *ns, NvmeCmd *cmd,
                           NvmeRequest *req)
{
    return nvme_rw(n, ns, cmd, req);
}

static uint16_t bb_io_cmd(FemuCtrl *n, NvmeNamespace *ns, NvmeCmd *cmd,
                          NvmeRequest *req)
{
    switch (cmd->opcode) {
    case NVME_CMD_READ:
    case NVME_CMD_WRITE:
        return bb_nvme_rw(n, ns, cmd, req);
    default:
        return NVME_INVALID_OPCODE | NVME_DNR;
    }
}

static uint16_t bb_admin_cmd(FemuCtrl *n, NvmeCmd *cmd)
{
    switch (cmd->opcode) {
    case NVME_ADM_CMD_FEMU_FLIP:
        bb_flip(n, cmd);
        return NVME_SUCCESS;
    default:
        return NVME_INVALID_OPCODE | NVME_DNR;
    }
}

int nvme_register_bbssd(FemuCtrl *n)
{
    n->ext_ops = (FemuExtCtrlOps) {
        .state            = NULL,
        .init             = bb_init,
        .exit             = NULL,
        .rw_check_req     = NULL,
        .admin_cmd        = bb_admin_cmd,
        .io_cmd           = bb_io_cmd,
        .get_log          = NULL,
    };

    return 0;
}

