/*
 * working on CentOS6.5, kernel 2.6.32-431.1.2.0.1.el6.x86_64
 */

#include "monitor.h"

/*
 * must be called before using struct vm_info, only need once
 */
void init_vm_info(struct vm_info *vminfo)
{
    vminfo->id = -1;
    vminfo->dp = NULL;
    vminfo->domname = NULL;
    memset(vminfo->blkname, '\0', BLKNAMEMAX);
    memset(vminfo->ifname, '\0', IFNAMEMAX);
    vminfo->fp = NULL;
    memset(vminfo->ipaddr, '\0', IPSZ);
}

void init_phy_info(struct phy_info *phyinfo)
{
    phyinfo->hostname_length = 20;
    phyinfo->hostname = 
        (char *)malloc(sizeof(char) * phyinfo->hostname_length);
    memset(phyinfo->hostname, '\0', phyinfo->hostname_length);
    phyinfo->fp = NULL;
}

/* 
 * print vm info 
 */
void print_vm_info(struct vm_info *vminfo)
{
    printf("%-4s | %-10s | %-8s | %-50s | %-8s | %-10s\n", 
            "ID", "domain", "name", "blkname", "ifname", "FILE *fp");
    printf("%-4d | %-10p | %-8s | %-50s | %-8s | %-10p\n", 
            vminfo->id, vminfo->dp, vminfo->domname, vminfo->blkname, 
            vminfo->ifname, vminfo->fp);
}

/*
 * print vm statistics
 */
void print_vm_statistics(struct vm_statistics *vm_stat, 
                         struct vm_info *vminfo)
{
    printf("%-8s: %-20s %-8s %-8s %-12s %-12s %-12s %-12s\n", 
            "domname", "cpu_time", "maxmem", "curmem", 
            "rd_bytes", "wr_bytes", "rx_bytes", "tx_bytes");
    printf("%-8s: %-20lld %-8ld %-8ld %-12lld %-12lld %-12lld %-12lld\n", 
            vminfo->domname, vm_stat->cpu_time, vm_stat->maxmem, 
            vm_stat->curmem, vm_stat->rd_bytes, vm_stat->wr_bytes, 
            vm_stat->rx_bytes, vm_stat->tx_bytes);
}

/* 
 * print vm workload
 */
void print_mach_load(struct mach_load *vmload)
{
    printf("%-6.2lf %-6.2lf %-8.2lf %-8.2lf %-8.2lf %-8.2lf\n", 
            vmload->cpu_load, vmload->mem_load,
            vmload->rd_load, vmload->wr_load, vmload->rx_load, 
            vmload->tx_load);
}

/* 
 * match the "source file" field to get the PATH of the virtual disk 
 * WARNING: currently, only consider the ONLY ONE virtual disk situation here
 * return 0 in case of success and -1 in case of error
 */
int get_vm_blkname(const char *xml, char *blkname)
{
    int i;
    char *p = strstr(xml, "source file");
    /* TODO: read each line of XML each time and then process it */
    if (NULL == p)
        return -1;

    char *q = NULL, *r = NULL;
    for (; p++; p) {
        if (*p == '\'') {
            q = p + 1;
            break;
        }
    }
    for (p = q; p++; p) {
        if (*p == '\'') {
            r = p - 1;
            break;
        }
    }
    for (i = 0, p = q; p <= r; p++, i++)
        blkname[i] = *p;
    return 0;
}

/* 
 * match the "vnet" field to get the PATH of the virtual interface 
 * WARNING: currently, only consider the ONLY ONE virtual interface 
 * situation here
 */
int get_vm_ifname(const char *xml, char *ifname)
{
    int i = 0;
    char *q = NULL, *r = NULL;
    char *p = strstr(xml, "vnet");
    if (NULL == p)
        return -1;

    for (q = p; *q != '\''; q++) 
        i++;
    for (i = 0, r = p; r < q; r++, i++)
        ifname[i] = *r;

    return 0;
}

/* 
 * This function read IP info of VMs from "ETC_HOSTS", so set it appropriately
 * before calling it. As is the common case, the line starts with "#" doesn't 
 * count return 0 in case of success(set ipaddr) and -1 in case of error(do 
 * nothing to ipaddr)
 */
int get_vm_ipaddr(char *ipaddr, const char *domname)
{
    char buff[1024];
    char name[DOMNAMEMAX];
    FILE *fp;
    char iptmp[IPSZ];

    if (NULL == (fp = fopen(ETC_HOSTS, "r"))) {
        perror("fopen /etc/hosts");
        exit(errno);
    }

    memset(buff, '\0', sizeof(buff));
    memset(name, '\0', sizeof(name));

    while (NULL != fgets(buff, sizeof(buff), fp)) {
        if (!strncmp(buff, "#", 1) || !strlen(buff)) {
            memset(buff, '\0', sizeof(buff));
            continue;
        }
        sscanf(buff, "%s%s",iptmp, name);
        if (!strncmp(domname, name, sizeof(name))) {
            //ipaddr = iptmp;
            strncpy(ipaddr, iptmp, sizeof(iptmp));
            break;
        }
        memset(buff, '\0', sizeof(buff));
    }

    if (!strncmp(ipaddr, iptmp, sizeof(iptmp)))
        return 0;
    else
        return -1;
}

/* use libvirt API to get VM memory rss */
void get_vm_rss(virDomainPtr dp, struct vm_statistics *vm_stat)
{
    int i;

    virDomainMemoryStatStruct stats[VIR_DOMAIN_MEMORY_STAT_NR];
    int nr_stats = 
        virDomainMemoryStats(dp, stats, VIR_DOMAIN_MEMORY_STAT_NR, 0);
    if (nr_stats == -1) {
        fprintf(stderr, "Failed to get VM memory statistics for");
        exit(errno);
    }
    for (i = 0; i < nr_stats; i++) 
        if (stats[i].tag == VIR_DOMAIN_MEMORY_STAT_RSS) {
            vm_stat->rss = stats[i].val;
            break;
        }
}

/* 
 * get_vm_static_info is responsible for getting static info of VMs, 
 * called once, and the info need not change vminfo->dp(gained by id) 
 * is already known, fill up other fields of vminfo 
 */
void get_vm_static_info(struct vm_info *vminfo)
{
    int ret;
    int i;

    /* domname field */
    vminfo->domname = virDomainGetName(vminfo->dp); 
    if (NULL == vminfo->domname) {
        fprintf(stderr, "failed to get hostname of domain with ID: %d\n", 
                vminfo->id);
        exit(VIR_ERR_UNKNOWN_HOST);
    }

    char *cap = (char *)malloc(sizeof(char) * DOMAIN_XML_SIZE);
    /* get XML configuration of each VM */
    cap = virDomainGetXMLDesc(vminfo->dp, VIR_DOMAIN_XML_UPDATE_CPU); 
    if (NULL == cap) {
        fprintf(stderr, "failed to get XMLDesc of domain: %s", 
                vminfo->domname);
        exit(VIR_ERR_XML_ERROR);
    }

    /* blkname field */
    if (-1 == get_vm_blkname(cap, vminfo->blkname)) {
        fprintf(stderr, "failed to get blkname of domain: %s", 
                vminfo->domname);
        exit(1);
    }

    /* ifname field */
    if (-1 == get_vm_ifname(cap, vminfo->ifname)) {
        fprintf(stderr, "failed to get ifname of domain: %s", 
                vminfo->domname);
        exit(1);
    }

    /* 
     * read info from host /etc/hosts to get VMs' IPs, domain name must be 
     * known before this operation because we suppose the host administrator 
     * had written VMs' ip along with the domain name to /etc/hosts file
     */
    if (-1 == get_vm_ipaddr(vminfo->ipaddr, vminfo->domname)) {
        fprintf(stderr, "WARNING: can't get IP address of domain[%s]", 
                vminfo->domname);
        exit(1);
    }

    /* fp filed */
    if (-1 == create_vm_rst_file(vminfo)) {
        fprintf(stderr, "can't create result file for domain[%s]", 
                vminfo->domname);
        exit(1);
    }

    free(cap);
}

void get_vm_cpustat(struct vm_statistics *vm_stat, struct vm_info *vminfo)
{
    int ret; 

    virDomainInfoPtr dominfo = (virDomainInfoPtr)malloc(sizeof(virDomainInfo));
    ret = virDomainGetInfo(vminfo->dp, dominfo);
    if (-1 == ret) {
        fprintf(stderr, "failed to get domaininfo of VMs\n");
        exit(VIR_FROM_DOM);
    }
    vm_stat->cpu_time = dominfo->cpuTime; /* in nanoseconds */
    vm_stat->maxmem = dominfo->maxMem;    /* in KiloBytes */
    vm_stat->curmem = dominfo->memory;    /* in KiloBytes */
}

/*
 * At each calling, the function connects to the guest agent(server) to gain
 * guest's memory usage through socket and then store the info.
 */
void get_vm_memstat(struct vm_statistics *vm_stat, struct vm_info *vminfo)
{
    int sockfd;
    struct sockaddr_in servaddr;
    int n, i;
    uint32_t recvbuf[NR_MEMINFO];

    /* socket */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "socket error\n");
        exit(errno);
    }

    /* initialize servaddr value of (sin_family, sin_port, sin_addr) */
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(L_PORT); /* port L_PORT is used by the server */
    if (inet_pton(AF_INET, vminfo->ipaddr, &servaddr.sin_addr) <= 0) {
        fprintf(stderr, "inet_pton error for %s", (strlen(vminfo->ipaddr) == 0) ? 
                "(null)" : vminfo->ipaddr);
        exit(errno);
    }

    /* connect */
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        fprintf(stderr, "connect error\n");
        exit(errno);
    }

    /* 
     * after calling connect successfully, the connection to server is 
     * established, we can operate on sockfd to get the information we want
     */
    if ((n = read(sockfd, recvbuf, sizeof(recvbuf))) > 0) {
        /* change back to host byte order first */
        for (i = 0; i < NR_MEMINFO; i++)
            recvbuf[i] = ntohl(recvbuf[i]);
        vm_stat->memtotal = recvbuf[MEMTOTAL];
        vm_stat->memfree = recvbuf[MEMFREE];
        vm_stat->buffers = recvbuf[BUFFERS];
        vm_stat->cached = recvbuf[CACHED];

       /* sscanf((char *)recvbuf, "%"PRIu32"%"PRIu32"%"PRIu32"%"PRIu32"", 
        *   &vm_stat->memtotal, &vm_stat->memfree, 
        *   &vm_stat->buffers, &vm_stat->cached); */
        printf("%"PRIu32"\n%"PRIu32"\n%"PRIu32"\n%"PRIu32"\n", 
                vm_stat->memtotal, vm_stat->memfree, 
                vm_stat->buffers, vm_stat->cached);
    }

}

void get_vm_blkstat(struct vm_statistics *vm_stat, struct vm_info *vminfo)
{
    int ret;

    virDomainBlockStatsPtr blkstat = 
        (virDomainBlockStatsPtr)malloc(sizeof(virDomainBlockStatsStruct));

    ret = virDomainBlockStats(vminfo->dp, vminfo->blkname, 
            blkstat, sizeof(virDomainBlockStatsStruct));

    if (-1 == ret) {
        fprintf(stderr, "failed to get domblkstats of VM [%s]\n", 
                vminfo->domname);
        exit(VIR_FROM_DOM);
    }

    /* only need rd/wr bytes data for the moment */
    vm_stat->rd_bytes = blkstat->rd_bytes;
    vm_stat->wr_bytes = blkstat->wr_bytes;

    free(blkstat);
}

void get_vm_ifstat(struct vm_statistics *vm_stat, struct vm_info *vminfo)
{
    int ret;

    virDomainInterfaceStatsPtr ifstat = 
        (virDomainInterfaceStatsPtr)malloc(sizeof(virDomainInterfaceStatsStruct));

    ret = virDomainInterfaceStats(vminfo->dp, vminfo->ifname, 
            ifstat, sizeof(virDomainInterfaceStatsStruct));

    if (-1 == ret) {
        fprintf(stderr, "failed to get domifstats of VM [%s]\n", 
                vminfo->domname);
        exit(VIR_FROM_DOM);
    }

    /* only need rx/tx bytes for the moment */
    vm_stat->rx_bytes = ifstat->rx_bytes;
    vm_stat->tx_bytes = ifstat->tx_bytes;

    free(ifstat);
}

/* collect VM's workload statistics */
void get_vm_workload(struct vm_statistics *vm_stat, struct vm_info *vminfo)
{
    int ret;

    /* get domaininfo of each active VM, including CPU and MEM infomation */
    //get_vm_dominfo(vm_stat, vminfo);

    /* get VM's cpu workload info */
    get_vm_cpustat(vm_stat, vminfo);

    /* get VM's memory workload info */
    get_vm_memstat(vm_stat, vminfo);

    /* WARNING: suppose we have only 1 vdisk here */
    get_vm_blkstat(vm_stat, vminfo);

    /* WARNINg: suppose we have only 1 virtual interface */
    get_vm_ifstat(vm_stat, vminfo);

    //print_vm_statistics(vm_stat, vminfo);
}

void compute_vm_load(struct mach_load *vmload, 
        struct vm_statistics *vm_stat_before, 
        struct vm_statistics *vm_stat_after, 
        ull microsec, unsigned long total_mem)
{

    ull delta_cpu_time = vm_stat_after->cpu_time - vm_stat_before->cpu_time;
    ull delta_rd_bytes = vm_stat_after->rd_bytes - vm_stat_before->rd_bytes;
    ull delta_wr_bytes = vm_stat_after->wr_bytes - vm_stat_before->wr_bytes;
    ull delta_rx_bytes = vm_stat_after->rx_bytes - vm_stat_before->rx_bytes;
    ull delta_tx_bytes = vm_stat_after->tx_bytes - vm_stat_before->tx_bytes;

    /* 
     * calculate the corresponding workload of each VM 
     * using the monitored statistics data 
     */

    /* 
     * (1). %CPU = 100 × delta_cpu_time / (time × nr_cores × 10^9) 
     * delta_cpu_time represent time differences domain get to run 
     * during time "time"
     */
    vmload->cpu_load = delta_cpu_time * 1.0 / 1000 / microsec * 100 / nr_cores;  

    /* (2). %MEM: use the rss size as the total used memory of VM */
    //vmload->mem_load = (vm_stat_after->rss) * 1.0 / total_mem * 100; 
    //vmload->mem_load = (vm_stat_after->curmem)  * 1.0 / total_mem * 100;
    vmload->mem_load = 
        100 - (double)(vm_stat_after->memfree + vm_stat_after->buffers +
        vm_stat_after->cached) / vm_stat_after->memtotal * 100.0;

    /* (3). vm disk read rate */
    vmload->rd_load = delta_rd_bytes * 1.0 / 1024 / microsec * 1000000;

    /* (4). vm disk write rate */
    vmload->wr_load = delta_wr_bytes * 1.0 / 1024 / microsec * 1000000;

    /* (5). vm net rx rate */
    vmload->rx_load = delta_rx_bytes * 1.0 / 1024 / microsec * 1000000;

    /* (6). vm net tx rate */
    vmload->tx_load = delta_tx_bytes * 1.0 / 1024 / microsec * 1000000;
}

int create_vm_rst_file(struct vm_info *vminfo)
{
    const char *suffix = ".rst";
    int domname_length = strlen(vminfo->domname);
    int fname_length = domname_length + strlen(suffix) + 1;
    char *fname = (char *)malloc(sizeof(char) * fname_length);

    memset(fname, '\0', fname_length);
    strncpy(fname, vminfo->domname, strlen(vminfo->domname));
    strncat(fname, suffix, strlen(suffix));
    vminfo->fp = fopen(fname, "w");

    /* set line buffer here */
    setlinebuf(vminfo->fp);

    if (NULL == vminfo->fp) {
        fprintf(stderr, "failed to create result file %s [%s]\n", 
                fname, strerror(errno));
        return -1;
    }

    free(fname);
    
    return 0;
}

void create_phy_rst_file(struct phy_info *phyinfo)
{
    const char *suffix = ".rst";
    gethostname(phyinfo->hostname, phyinfo->hostname_length);
    int hname_len = strlen(phyinfo->hostname);
    int fname_length = hname_len + strlen(suffix) + 1;
    char *fname = (char *)malloc(sizeof(char) * fname_length);
    memset(fname, '\0', fname_length);
    strncpy(fname, phyinfo->hostname, hname_len);
    strncat(fname, suffix, strlen(suffix));
    phyinfo->fp = fopen(fname, "w");
    setlinebuf(phyinfo->fp);
    if (NULL == phyinfo->fp) {
        fprintf(stderr, "failed to create result file %s [%s]\n", 
                fname, strerror(errno));
        exit(errno);
    }
}

int match_nic(char *buffer, char *nic)
{
    int nic_length = strlen(nic);
    int buf_size = strlen(buffer);
    int i = 0, j = 0;
    
    for (i = 0; (i < buf_size) && ((i-j) < nic_length); i++) {
        if (buffer[i] == ' ') {
            j++;
            continue;
        }
        if (buffer[i] == nic[i-j])
            continue;
        else
            break;
    }
    if (buffer[i] == ':')
        return 1;
    else
        return 0;
}

void extract_nic_bytes(char *buffer, ull *rx_bytes, ull *tx_bytes)
{
    int i, j, k;
    int has_space = 1;
    char rx[100] = {'\0'};
    int buf_size = strlen(buffer);
    
    for (i = 0; i < buf_size; i++)
        if (buffer[i] == ':')
            break;
    /* 
     * do no more checking here for the noexistence situation of wrong 
     * ethernet line 
     */
    i++;
    if (buffer[i] != ' ')
        has_space = 0;
    if (has_space) {
        sscanf(buffer, "%*s %lld %*d %*d %*d %*d %*d %*d %*d %lld", 
                rx_bytes, tx_bytes);
    }
    else {
        k = 0;
        for (j = i; buffer[j] != ' '; j++)
            rx[k++] = buffer[j];
        *rx_bytes = atol(rx);
        sscanf(buffer, "%*s %*d %*d %*d %*d %*d %*d %*d %llu", tx_bytes);
        //printf("in extract_nic_bytes, rx=%ld, tx=%ld\n", *rx_bytes, *tx_bytes);
    }
    
    //printf("rx=%ld, tx=%ld\n", *rx_bytes, *tx_bytes); 
}


/* 
 * get cpu info from /proc/stat
 */
void get_phy_cpustat(struct phy_statistics *phy_stat)
{
    char line[8192];
    FILE *fp = fopen("/proc/stat", "r");
    if (NULL == fp) {
        fprintf(stderr, "failed to open /proc/stat [%s]\n", strerror(errno));
        exit(errno);
    }

    memset(line, 0, 8192);
    if (NULL == fgets(line, sizeof(line), fp)) {
        fprintf(stderr, "failed to read info from /proc/stat [%s]\n", 
                strerror(errno));
        exit(errno);
    }

    sscanf(line, "%*s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu\n", 
            &phy_stat->user, 
            &phy_stat->nice, 
            &phy_stat->system,
            &phy_stat->idle, 
            &phy_stat->iowait, 
            &phy_stat->irq, 
            &phy_stat->softirq, 
            &phy_stat->steal, 
            &phy_stat->guest,
            &phy_stat->guest_nice);

    fclose(fp);
}

void get_phy_memstat(struct phy_statistics *phy_stat)
{
    int i;
    FILE *fp = NULL;
    for (i = 0; i < 100; i++) {
        fp = fopen("/proc/meminfo", "r");
        if (fp)
            break;
    }
    if (NULL == fp) { 
        fprintf(stderr, "failed to open /proc/meminfo [%s]\n", strerror(errno));
        exit(errno);
    }
    fscanf(fp, "%*s %ld %*s\n%*s %ld %*s\n%*s %ld %*s\n%*s %ld", 
            &phy_stat->memtotal, &phy_stat->memfree, &phy_stat->buffers, 
            &phy_stat->cached);
    fclose(fp);
}

/*
 * user must handle errors by themselves
 */
void get_phy_blkstat(struct phy_statistics *phy_stat, const char *disk)
{
    char buf[100] = {'\0'};
    char device[20] = {'\0'};
    FILE *fp = fopen("/proc/diskstats", "r");
    if (NULL == fp) {
        fprintf(stderr, "faild to open /proc/diskstats[%s]\n", strerror(errno));
        exit(errno);
    }
    while (fgets(buf, sizeof(buf), fp)) {
        sscanf(buf, "%*d %*d %s", device);
        if (0 == strcmp(disk, device)) {
            sscanf(buf, "%*u %*u %*s %*u %*u %Lu %*u %*d %Lu", 
                    &phy_stat->rd_sectors, &phy_stat->wr_sectors);
            break;
        }
        memset(device, '\0', sizeof(device));
        memset(buf, '\0', sizeof(buf));
    }

    fclose(fp);
}

void get_phy_ifstat(struct phy_statistics *phy_stat, const char *nic)
{
    char buf[1024] = {'\0'};
    char device[20] = {'\0'}; 
    int match = 0;

    FILE *fp = fopen("/proc/net/dev", "r");
    if (NULL == fp) {
        fprintf(stderr, "faild to open /proc/net/dev [%s]\n", strerror(errno));
        exit(errno);
    }
    while (fgets(buf, sizeof(buf), fp)) {
        if (match_nic(buf, ETHERNET)) {
            match = 1;
            break;
        }
        memset(buf, '\0', sizeof(buf));
    }
    if (match == 0) {
        fprintf(stderr, "couldn't resolve [%s] in /proc/net/dev, "
                "make sure you have the NIC\n", ETHERNET);
        exit(errno);
    }
    //printf("%s\n", buf);
    extract_nic_bytes(buf, &phy_stat->rx_bytes, &phy_stat->tx_bytes);

    fclose(fp);
}

void get_phy_workload(struct phy_statistics *phy_stat)
{
    char *disk = DISK;
    char *nic = ETHERNET;

    /* get physical cpu statistics */
    get_phy_cpustat(phy_stat);

    /* get physical memory statistics */
    get_phy_memstat(phy_stat);

    /* get physical disk statistics */
    get_phy_blkstat(phy_stat, disk);

    /* get physical network statistics */
    get_phy_ifstat(phy_stat, nic);
}

/* 
 * sum up user+nice+sys+iowait+idle+irq+softirq+steal, 
 * precluding guest and guest_nice 
 */
ull sum_cpu_stats(struct phy_statistics *phy_stats)
{
    ull cpu_ticks = 
        phy_stats->user + 
        phy_stats->nice + 
        phy_stats->system + 
        phy_stats->idle + 
        phy_stats->iowait + 
        phy_stats->irq + 
        phy_stats->softirq +
        phy_stats->steal /*+ 
        phy_stats->guest + 
        phy_stats->guest_nice*/;
    return cpu_ticks;
}

/*
 * caculate physical machine's workload
 */
void compute_phy_load(struct mach_load *phyload, 
        struct phy_statistics *phy_stat_before, 
        struct phy_statistics *phy_stat_after, 
        long microsec)
{
    ull cpu_ticks_before = sum_cpu_stats(phy_stat_before);
    ull cpu_ticks_after = sum_cpu_stats(phy_stat_after);

    /* 
     * total elapsed cpu time in ticks, equavelent to the difference 
     * of gettimeofday 
     */
    ull delta_cpu_time = cpu_ticks_after - cpu_ticks_before; 
    ull delta_idle_time = phy_stat_after->idle - phy_stat_before->idle;

    /* get memory unused(including the "free+buffers+cached" */
    unsigned long mem_unused = phy_stat_after->memfree + 
        phy_stat_after->buffers + phy_stat_after->cached;

    ull delta_rd_bytes = 
        (phy_stat_after->rd_sectors - phy_stat_before->rd_sectors) * 512;
    ull delta_wr_bytes = 
        (phy_stat_after->wr_sectors - phy_stat_before->wr_sectors) * 512;

    ull delta_rx_bytes = phy_stat_after->rx_bytes - phy_stat_before->rx_bytes;
    ull delta_tx_bytes = phy_stat_after->tx_bytes - phy_stat_before->tx_bytes;

    /* 
     * calculate the corresponding workload of each VM 
     * using the monitored statistics data 
     */

    /* 
     * (1) CPU%: we take non-idle cpu time as the time cpu 
     *  busying running tasks  
     *  [1~100(%)]
     */
    phyload->cpu_load = 100 - delta_idle_time * 1.0 / delta_cpu_time * 100; 

    /* 
     * (2) MEM%: get the memory usage at the time poniter 
     *  [1~100(%)]
     */
    phyload->mem_load = 
        100 - mem_unused * 1.0 / phy_stat_after->memtotal * 100; 

    /* 
     * (3) DISK read rate
     *  [in Kbps]
     */
    phyload->rd_load = delta_rd_bytes * 1.0 / 1024 / microsec * 1000000; 

    /* 
     * (4) DISK write rate 
     *  [in Kbps]
     */
    phyload->wr_load = delta_wr_bytes * 1.0 / 1024 / microsec * 1000000; 

    /* 
     * (5) NET rx rate 
     *  [in Kbps]
     */
    phyload->rx_load = delta_rx_bytes * 1.0 / 1024 / microsec * 1000000; 

    /* 
     * (6) NET tx rate 
     *  [in Kbps]
     */
    phyload->tx_load = delta_tx_bytes * 1.0 / 1024 / microsec * 1000000; 
}