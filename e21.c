#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/interrupt.h>


/*
	2014.03.30 intel i82551 - czyli e100
	proba napisania w calosci od zera drivera dla i82551 pod qemu
*/


/*
 - 11.c - wreszcie udalo sie wykonac 2 komendy: NOP i IAS (individual address setup)
 - 14.c - pierwsza wersja, ktora cokolwiek wysyla, na razie spod zlego adresu ale wysyla
 - 16.c - pierwsza wersja gdie nadawane jest cto co ja chcialem



*/



// komendy CUC str. 38
#define CUC_NOP		(0 << 4)
#define CUC_START	(1 << 4)
#define CUC_BASE 	(6 << 4)


// komendy RUC str. 39
#define RUC_NOP		(0 << 16)
#define RUC_START	(1 << 16)
#define RUC_BASE	(6 << 16)


// komendy CBL - str. 57
#define CBL_NOP		(0 << 16)
#define CBL_IAS		(1 << 16)
#define CBL_CONF	(2 << 16)
#define CBL_TX		(4 << 16)

// maski przerwan - str. 37
#define	CX		(1 << (31 - 16))
#define FR		(1 << (30 - 16))
#define CNA		(1 << (29 - 16))
#define RNR		(1 << (28 - 16))
#define ER		(1 << (27 - 16))
#define FCP		(1 << (26 - 16))

//#define IRQ_MASK 	(CX | FR | CNA | RNR | ER | FCP)
#define IRQ_MASK 	(CX | CNA | RNR | ER | FCP)


u32 * regs;
u8 * regs8;
resource_size_t pciaddr;
u8 *cbl = NULL;
u8 *rfd = NULL;

// str.32
struct csr {
	u16 scb_status;
	u16 scb_cmd;
	u32 scb_gp;
	u32 port;
	u16 res1;
	u16 eeprom;
	u16 mdi_cr;
} *csr;

// str. 59
struct cb_nop {
	u32 dword0;
	u32 link_addr;
} cb_nop;

// str. 60
struct cb_ias {
	u32 dword0;
	u32 link_addr;
	u8 mac1, mac2, mac3, mac4, mac5, mac6;
	u16 empty;
} cb_ias;


struct cb_tx {
	u32 dword0;
	u32 link_addr;
	u32 tbda;
	u16 tcbbc;
	u8  tx_thr;
	u8  tbd_num;
	//u8  data[32];
	
	u32 tbd_tba;
	u32 tbd_size;
} cb_tx;


// str. 100
struct cb_rx {
	u32 dword0;
	u32 link_addr;
	u32 reserved;
	u16 count;
	u16 size;
} cb_rx;

// str. 62
struct cb_conf {
	u32 dword0;
	u32 link_addr;
	u8 byte0;
	u8 byte1;
	u8 byte2;
	u8 byte3;
	u8 byte4;
	u8 byte5;
	u8 byte6;
	u8 byte7;
	u8 byte8;
	u8 byte9;
	u8 byte10;
	u8 byte11;
	u8 byte12;
	u8 byte13;
	u8 byte14;
	u8 byte15;
	u8 byte16;
	u8 byte17;
	u8 byte18;
	u8 byte19;
	u8 byte20;
	u8 byte21;
} cb_conf;


// dla werfyikacji offsetow w strukturze
void print_csr_offs (void)
{
	printk("off scb_status: %u\n", offsetof(struct csr, scb_status));
	printk("off scb_cmd: %u\n", offsetof(struct csr, scb_cmd));
	printk("off scb_gp: %u\n", offsetof(struct csr, scb_gp));
	printk("off port: %u\n", offsetof(struct csr, port));
	printk("off eeprom: %u\n", offsetof(struct csr, eeprom));
	printk("off mdi_cr: %u\n", offsetof(struct csr, mdi_cr));
}



void print_csr (void)
{
	printk("\n");
	printk("scb_status: %#x\n", csr->scb_status);
	printk("scb_cmd: %#x\n", csr->scb_cmd);
	printk("scb_gp: %#x\n", csr->scb_gp);
	printk("port: %#x\n", csr->port);
	printk("eeprom: %#x\n", csr->eeprom);
	printk("mdi_cr: %#x\n", csr->mdi_cr);
	printk("\n");
}


// str. 39
void cuc_set_base (void)
{
//	csr->scb_gp = 0x22446688;		// tu ma byc adres CBL
	csr->scb_gp = (u32)0x0;			// 0 - bo ne chcemy uzywac segmentacji - zerujemy rej segmentu
	csr->scb_cmd = CUC_BASE | IRQ_MASK;
	printk("cuc scb_gp: %#x\n", csr->scb_gp);
}

void cuc_start (void)
{
	csr->scb_gp = virt_to_phys(cbl);		// tu ma byc adres CBL
	csr->scb_cmd = CUC_START | IRQ_MASK;
}



void ruc_set_base (void)
{
	csr->scb_gp = (u32)0x0;			// 0 - bo ne chcemy uzywac segmentacji - zerujemy rej segmentu
	csr->scb_cmd = RUC_BASE | IRQ_MASK;
	printk("ruc scb_gp: %#x\n", csr->scb_gp);
}


void ruc_start (void)
{
	csr->scb_gp = virt_to_phys(rfd);		// tu ma byc adres CBL
	csr->scb_cmd = RUC_START | IRQ_MASK;
}







void ack_irq (void)
{
	u32 val;
	static u8 x = 0;

	val = csr->scb_status;

//	if (x < 5 )
	if (val != 0)
		printk("ack1: %#x\n", val >> 8);	

	x++;

	csr->scb_status = val;

	val = csr->scb_status;
//	printk("ack2: %#x\n", val >> 8);	

}


// str. 59
void cb_insert_nop (void)
{
	u32 el, s, i, cmd;

	printk("cb_nop: %#x\n", &cb_nop);

	el = 1 << 31;
	s = 0 << 30;
	i = 0 << 29;
	cmd = CBL_NOP;
	
	
	cb_nop.dword0 = el | s | i | cmd;
	cb_nop.link_addr = 0x0;

	//teraz skopiuj te strukture do listy CBL
	memcpy(cbl, &cb_nop, sizeof(struct cb_nop));

	printk("CBL VALS: (%#x) %#x %#x\n", cbl, *(u32 *)cbl, *(u32 *)(cbl + 4));

}


void cb_insert_ias (void)
{
	u32 el, s, i, cmd;

	printk("cb_ias: %#x\n", &cb_ias);

	el = 1 << 31;
	s = 0 << 30;
	i = 0 << 29;
	cmd = CBL_IAS;
	
	
	cb_ias.dword0 = el | s | i | cmd;
	cb_ias.link_addr = 0x0;
	cb_ias.mac1 = 0x00;
	cb_ias.mac2 = 0x11;
	cb_ias.mac3 = 0x22;
	cb_ias.mac4 = 0x33;
	cb_ias.mac5 = 0x44;
	cb_ias.mac6 = 0x55;

	//teraz skopiuj te strukture do listy CBL
	memcpy(cbl, &cb_ias, sizeof(struct cb_ias));

	printk("CBL VALS: (%#x) %#x %#x\n", cbl, *(u32 *)cbl, *(u32 *)(cbl + 4));

}


u8 *packet;

// str. 83
void cb_insert_tx (void)
{
	u32 el, s, i, cid, nc, sf, cmd, c, ok, u;

	printk("cb_tx: %#x\n", &cb_tx);

	el = 1 << 31;
	s = 0 << 30;
	i = 0 << 29;
	cid = 0 << 24;
	nc = 0 << 20;
	sf = 0 << 19;
	cmd = CBL_TX;
	c = 0 << 15;
	ok = 0 << 13;
	u = 0 << 12;
	
	packet = kmalloc(1024, GFP_KERNEL);
	if (packet == NULL) {
		printk("no mem for packet\n");
		return;
	}

	
//	memcpy(&cb_tx.data, "proba nadawania", 16);
//	memcpy(&cb_tx.data, "aaaaaaaaaaaaaaa", 16);
	memcpy(packet, "proba nadawania", 16);

	cb_tx.dword0 = el | s | i | cid | nc | sf | cmd | c | ok | u;
	cb_tx.link_addr = 0x00;
	cb_tx.tbda = 0xffffffff;			// NULL pointer
	cb_tx.tbd_num = 0;
	cb_tx.tx_thr = 1;			//tx threshold - 1=8 bajtow
	cb_tx.tcbbc = (1 << 15) | strlen(packet);

	cb_tx.tbd_tba = virt_to_phys(packet);
	cb_tx.tbd_size = (1 << 16) | strlen(packet);

	//teraz skopiuj te strukture do listy CBL
	memcpy(cbl, &cb_tx, sizeof(struct cb_tx));

	printk("CBL VALS: (%#x) %#x %#x\n", cbl, *(u32 *)cbl, *(u32 *)(cbl + 4));


	kfree(packet);			// TYMCZASOWO - DOCELOWO ZWALNIAC PAMIEC W IRQ_HANDLER!

}


void init_rx (void)
{
	u32 el, s, h, sf, c, ok;

	printk("cb_rx: %#x\n", &cb_rx);

	el = 1 << 31;
	s = 0 << 30;
	h = 0 << 20;
	sf = 0 << 19;
	c = 0 << 15;
	ok = 0 << 13;
	
	packet = kmalloc(1024, GFP_KERNEL);
	if (packet == NULL) {
		printk("no mem for packet\n");
		return;
	}

	

	cb_rx.dword0 = el | s | h | sf | c | ok;
	cb_rx.link_addr = 0x00;

	cb_rx.size = 1536;			// wielkosc bufora?
	cb_rx.count = 0;			// to chyba wpisuje device

	//teraz skopiuj te strukture do listy CBL
	memcpy(cbl, &cb_tx, sizeof(struct cb_tx));




}


void cb_insert_conf (void)
{
	u32 el, s, i, cmd, c, ok;


	el = 1 << 31;
	s = 0 << 30;
	i = 0 << 29;
	cmd = CBL_CONF;
	c = 0 << 15;
	ok = 0 << 13;
	
	
	cb_conf.dword0 = el | s | i | cmd | c | ok;
	cb_conf.link_addr = 0x0;
	cb_conf.byte0 = 16;		// 16 bo tylko do bajtu15 - promisc
	cb_conf.byte1 = 0x08;
	cb_conf.byte2 = 0x0;
	cb_conf.byte3 = 0x0;
	cb_conf.byte4 = 0x0;
	cb_conf.byte5 = 0x0;
	cb_conf.byte6 = 0xf2;
	cb_conf.byte7 = 0x1;
	cb_conf.byte8 = 0x0;
	
	cb_conf.byte9 = 0x0;
	cb_conf.byte10 = 0x28;
	cb_conf.byte11 = 0x0;
	cb_conf.byte12 = 0x60;
	cb_conf.byte13 = 0x0;
	cb_conf.byte14 = 0xf2;
	cb_conf.byte15 = 0x81;		// promisc

/*	cb_conf.byte16 = 0x;
	cb_conf.byte17 = 0x;
	cb_conf.byte18 = 0x;
	cb_conf.byte19 = 0x;
	cb_conf.byte20 = 0x;
	cb_conf.byte21 = 0x;
*/
	//teraz skopiuj te strukture do listy RFD
	memcpy(cbl, &cb_ias, sizeof(struct cb_ias));

	printk("CBL VALS: (%#x) %#x %#x\n", cbl, *(u32 *)cbl, *(u32 *)(cbl + 4));

}







static irqreturn_t irq_handler (int irq, void *dev_id, struct pt_regs *regs)
{
	u32 val;

	val = csr->scb_status;
	if (val == 0)
		return IRQ_NONE;

	ack_irq();

	printk("irq\n");

	return IRQ_HANDLED;
}


void init_hw (void)
{

//	print_csr_offs();			// sprawdz offsety w strukturze

	init_rx();


//	print_csr();
	cuc_set_base();
	ruc_set_base();

//	cb_insert_nop();

	cb_insert_conf();

//	cb_insert_ias();
	cb_insert_tx();

//	mdelay(1000);

	cuc_start();
	ruc_start();

	print_csr();




}



int e100wk_probe (struct pci_dev *dev, const struct pci_device_id *id)
{
	//u32 tcr, tcrs;
	int st;
	//u8 mac[6], x;
	
	printk("%s\n", __FUNCTION__);
	printk("vendor:device: %x:%x\n", dev->vendor, dev->device);
	printk("class: %x\n", dev->class);
	printk("revision: %x\n", dev->revision);
	printk("pin: %x\n", dev->pin);
	printk("irq: %d\n", dev->irq);
	printk("is_busmaster: %d\n", dev->is_busmaster);


	st = pci_enable_device(dev);
	printk("pci enable dev: %u\n", st);

	pciaddr = pci_resource_start(dev, 0);				
	if (pciaddr == 0) {
		printk("error pci_resource_start\n");
		return 1;
	}
	printk("pciaddr: %#x\n", pciaddr);

/*
	st = pci_request_region(dev, 0, "aaa");
	if (st == 0)
		printk("pci req reg ok\n");
	else 
		printk("pci req reg err\n");
*/



	regs = pci_ioremap_bar(dev, 0);
//	regs = ioremap(pciaddr, 16);
	if (regs == NULL) {
		printk("error ioremap\n");
		return 1;
	}

	regs8 = (u8 *)regs;
	csr = (struct csr *)regs;

	printk("regs: %#x\n", regs);




	switch (dev->revision) {
		case 0xf:	printk("i82551\n"); 
				break;
		default:	printk("other i8255x\n");
				break;
	}

	pci_set_master(dev);

	st = request_irq(dev->irq, irq_handler, IRQF_SHARED, "e100wk", dev);
	printk("request_irq: %d\n", st);

//	cbl = kmalloc(1024, GFP_KERNEL);
	cbl = kmalloc(1024, GFP_DMA);
	if (cbl == NULL) {
		printk("kmalloc: no mem for cbl\n");
		return 1;
	}
	printk("CBL: %#x => %#x\n", cbl, virt_to_phys(cbl));


	rfd = kmalloc(65536, GFP_DMA);
	if (cbl == NULL) {
		printk("kmalloc: no mem for rfd\n");
		return 1;
	}
	printk("RFD: %#x => %#x\n", rfd, virt_to_phys(rfd));




	init_hw();


	return 0;
}






void e100wk_remove (struct pci_dev *dev)
{
	printk("%s\n", __FUNCTION__);

	free_irq(dev->irq, dev);

	iounmap(regs);			// tak sie tez zwlania po pic_ioremap_bar zdaje sie

	if (cbl != NULL)
		kfree(cbl);
	
	if (rfd != NULL)
		kfree(rfd);

}










DEFINE_PCI_DEVICE_TABLE(e100wk_id) = {
	{0x8086, 0x1209, PCI_ANY_ID, PCI_ANY_ID},
	{0}
};


struct pci_driver e100wk_driver = {
	.name 		= "e100wk",
	.id_table	= e100wk_id,
	.probe		= e100wk_probe,
	.remove		= e100wk_remove


};






static int __init r_init (void)
{
	int st;

	st = pci_register_driver(&e100wk_driver);
//	printk("pci_register_driver st: %u\n", st);

        return st;
}




static void __exit r_exit(void)
{
	pci_unregister_driver(&e100wk_driver);
}






MODULE_LICENSE("GPL");
module_init(r_init);
module_exit(r_exit);

