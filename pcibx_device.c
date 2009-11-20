/*

  Catalyst PCIBX32 PCI Extender control utility

  Copyright (c) 2006-2009 Michael Buesch <mb@bu3sch.de>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
  Boston, MA 02110-1301, USA.

*/

#include "pcibx_device.h"
#include "pcibx.h"
#include "utils.h"

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#ifdef __linux__
# include <linux/ppdev.h>
#endif

/* Parport control register bits */
#define PPCTL_IRQEN	(1 << 4)
#define PPCTL_READ	(1 << 5)
#define PPCTL_DATAMASK	0xF


static uint8_t parport_read_data(struct pcibx_device *dev)
{
	uint8_t res = 0;

#if defined(__linux__)
	if (ioctl(dev->fd, PPRDATA, &res))
		prerror("Failed to read the parallel port data register\n");
#else
# error "Operating system not supported"
#endif

	return res;
}

static void parport_write_data(struct pcibx_device *dev, uint8_t value)
{
#if defined(__linux__)
	if (ioctl(dev->fd, PPWDATA, &value))
		prerror("Failed to write the parallel port data register\n");
#else
# error "Operating system not supported"
#endif
}

static void parport_write_control(struct pcibx_device *dev,
				  uint8_t mask, uint8_t value)
{
#if defined(__linux__)
	struct ppdev_frob_struct frob = {
		.mask = mask,
		.val = value,
	};
	int direction;

	if (mask & PPCTL_READ) {
		direction = !!(value & PPCTL_READ);
		if (ioctl(dev->fd, PPDATADIR, &direction))
			prerror("Failed to set parallel port data direction\n");
	}
	frob.mask &= ~PPCTL_READ;
	frob.val &= frob.mask;
	if (ioctl(dev->fd, PPFCONTROL, &frob))
		prerror("Failed to write the parallel port control register\n");
#else
# error "Operating system not supported"
#endif
}

static int parport_open(struct pcibx_device *dev, const char *port)
{
#if defined(__linux__)
	int err;

	dev->fd = open(port, O_RDWR);
	if (dev->fd < 0) {
		prerror("Could not open parallel port %s: %s\n",
			port, strerror(errno));
		return -1;
	}
//FIXME
#if 0
	err = ioctl(dev->fd, PPEXCL);
	if (err) {
		prerror("Failed to gain exclusive access to the parallel port %s: %s\n",
			port, strerror(err < 0 ? -err : err));
		close(dev->fd);
		return -1;
	}
#endif
	err = ioctl(dev->fd, PPCLAIM);
	if (err) {
		prerror("Failed to claim the parallel port %s: %s\n",
			port, strerror(err < 0 ? -err : err));
		close(dev->fd);
		return -1;
	}
#else
# error "Operating system not supported"
#endif

	parport_write_control(dev, PPCTL_DATAMASK | PPCTL_READ | PPCTL_IRQEN, 0xE);

	return 0;
}

static void parport_close(struct pcibx_device *dev)
{
#if defined(__linux__)
	ioctl(dev->fd, PPRELEASE);
	close(dev->fd);
#else
# error "Operating system not supported"
#endif
}

static void pcibx_set_address(struct pcibx_device *dev,
			      uint8_t address)
{
	parport_write_control(dev, PPCTL_DATAMASK, 0xE);
	parport_write_data(dev, address + dev->regoffset);
	parport_write_control(dev, PPCTL_DATAMASK, 0x6);
	udelay(100);
	parport_write_control(dev, PPCTL_DATAMASK, 0xE);
}

static void pcibx_write_data(struct pcibx_device *dev,
			     uint8_t data)
{
	parport_write_data(dev, data);
	parport_write_control(dev, PPCTL_DATAMASK, 0xC);
	udelay(100);
	parport_write_control(dev, PPCTL_DATAMASK, 0xE);
}

static void pcibx_write_data_ext(struct pcibx_device *dev,
				 uint8_t data)
{
	parport_write_control(dev, PPCTL_DATAMASK, 0xE);
	parport_write_data(dev, data);
	parport_write_control(dev, PPCTL_DATAMASK, 0xC);
	msleep(2);
	parport_write_control(dev, PPCTL_DATAMASK, 0xE);
}

static uint8_t pcibx_read_data(struct pcibx_device *dev)
{
	uint8_t v;

	parport_write_control(dev, PPCTL_DATAMASK | PPCTL_READ,
			      PPCTL_READ | 0xF);
	v = parport_read_data(dev);
	parport_write_control(dev, PPCTL_DATAMASK | PPCTL_READ, 0xE);

	return v;
}

static void pcibx_write(struct pcibx_device *dev,
			uint8_t reg,
			uint8_t value)
{
	pcibx_set_address(dev, reg);
	pcibx_write_data(dev, value);
}

static void pcibx_write_ext(struct pcibx_device *dev,
			    uint8_t reg,
			    uint8_t value)
{
	pcibx_set_address(dev, reg);
	pcibx_write_data_ext(dev, value);
}

static uint8_t pcibx_read(struct pcibx_device *dev,
			       uint8_t reg)
{
	pcibx_set_address(dev, reg);
	return pcibx_read_data(dev);
}

int pcibx_device_init(struct pcibx_device *dev,
		      const char *port,
		      int is_pci1)
{
	memset(dev, 0, sizeof(*dev));
	if (is_pci1)
		dev->regoffset = PCIBX_REGOFFSET_PCI1;
	else
		dev->regoffset = PCIBX_REGOFFSET_PCI2;

	return parport_open(dev, port);
}

void pcibx_device_exit(struct pcibx_device *dev)
{
	parport_close(dev);
	memset(dev, 0, sizeof(*dev));
}

static void prsendinfo(const char *command)
{
	if (cmdargs.verbose >= 2)
		prinfo("Sending command: %s\n", command);
}

void pcibx_cmd_global_pwr(struct pcibx_device *dev, int on)
{
	if (on) {
		prsendinfo("Global Power ON");
		pcibx_write(dev, PCIBX_REG_GLOBALPWR, 1);
	} else {
		prsendinfo("Global Power OFF");
		pcibx_write(dev, PCIBX_REG_GLOBALPWR, 0);
	}
}

void pcibx_cmd_uut_pwr(struct pcibx_device *dev, int on)
{
	if (on) {
		pcibx_cmd_global_pwr(dev, 1);
		prsendinfo("UUT Voltages ON");
		pcibx_write(dev, PCIBX_REG_UUTVOLT, 0);
		/* Wait for the RST# to become de-asserted. */
		do {
			msleep(200);
		} while (!(pcibx_read(dev, PCIBX_REG_STATUS) & PCIBX_STATUS_RSTDEASS));
	} else {
		prsendinfo("UUT Voltages OFF");
		pcibx_write(dev, PCIBX_REG_UUTVOLT, 1);
	}
}

uint8_t pcibx_cmd_getboardid(struct pcibx_device *dev)
{
	prsendinfo("Get board ID");
	return pcibx_read(dev, PCIBX_REG_BOARDID);
}

uint8_t pcibx_cmd_getfirmrev(struct pcibx_device *dev)
{
	prsendinfo("Get firmware rev");
	return pcibx_read(dev, PCIBX_REG_FIRMREV);
}

uint8_t pcibx_cmd_getstatus(struct pcibx_device *dev)
{
	prsendinfo("Get status bits");
	return pcibx_read(dev, PCIBX_REG_STATUS);
}

void pcibx_cmd_clearbitstat(struct pcibx_device *dev)
{
	prsendinfo("Clear 32/64 bit status");
	pcibx_write(dev, PCIBX_REG_CLEARBITSTAT, 0);
}

void pcibx_cmd_aux5(struct pcibx_device *dev, int on)
{
	if (on) {
		prsendinfo("Aux 5V ON");
		pcibx_write(dev, PCIBX_REG_AUX5V, 0);
	} else {
		prsendinfo("Aux 5V OFF");
		pcibx_write(dev, PCIBX_REG_AUX5V, 1);
	}
}

void pcibx_cmd_aux33(struct pcibx_device *dev, int on)
{
	if (on) {
		prsendinfo("Aux 3.3V ON");
		pcibx_write(dev, PCIBX_REG_AUX33V, 0);
	} else {
		prsendinfo("Aux 3.3V OFF");
		pcibx_write(dev, PCIBX_REG_AUX33V, 1);
	}
}

float pcibx_cmd_sysfreq(struct pcibx_device *dev)
{
	float mhz;
	uint32_t tmp;
	uint32_t v;

	prsendinfo("Measure system frequency");
	pcibx_write(dev, PCIBX_REG_FREQMEASURE_CTL, 1);
	msleep(15);
	v = pcibx_read(dev, PCIBX_REG_FREQMEASURE_0);
	tmp = v;
	v = pcibx_read(dev, PCIBX_REG_FREQMEASURE_1);
	tmp |= (v << 8);
	v = pcibx_read(dev, PCIBX_REG_FREQMEASURE_2);
	tmp |= (v << 16);

	mhz = (float)tmp * 100.0 / 1048575.0;

	return mhz;
}

float pcibx_cmd_measure(struct pcibx_device *dev, enum measure_id id)
{
	float ret;
	int i;
	uint16_t d0, d1;
	uint16_t tmp;

	prsendinfo("Measuring V/A");
	pcibx_write(dev, PCIBX_REG_MEASURE_CTL, id);
	msleep(10);
	pcibx_write_ext(dev, PCIBX_REG_MEASURE_CONV, 0);
	msleep(2);
	pcibx_set_address(dev, PCIBX_REG_MEASURE_STROBE);
	for (i = 0; i < 13; i++)
		pcibx_write_data(dev, 0);
	d0 = pcibx_read(dev, PCIBX_REG_MEASURE_DATA0);
	d1 = pcibx_read(dev, PCIBX_REG_MEASURE_DATA1);
	tmp = d0;
	tmp |= (d1 << 8);

	if (id == MEASURE_V12UUT)
		ret = (float)tmp * 5.75 * 2.5 / 4096.0;
	else
		ret = (float)tmp * 2.26 * 2.5 / 4096.0;

	return ret;
}

void pcibx_cmd_ramp(struct pcibx_device *dev, int fast)
{
	prsendinfo("+5V RAMP");
	pcibx_write(dev, PCIBX_REG_RAMP, fast ? 1 : 0);
}

void pcibx_cmd_rst(struct pcibx_device *dev, double sec)
{
	uint32_t tmp;

	prsendinfo("RST#");
	sec /= 2.56;
	sec *= 1000000;
	tmp = sec;
	tmp &= 0x1FFFFF;
	tmp |= (1 << 23);
	pcibx_write(dev, PCIBX_REG_RST_0, (tmp & 0x000000FF));
	pcibx_write(dev, PCIBX_REG_RST_1, (tmp & 0x0000FF00) >> 8);
	pcibx_write(dev, PCIBX_REG_RST_2, (tmp & 0x00FF0000) >> 16);
}

void pcibx_cmd_rstdefault(struct pcibx_device *dev)
{
	prsendinfo("default RST#");
	pcibx_write(dev, PCIBX_REG_RST_0, 0);
	pcibx_write(dev, PCIBX_REG_RST_1, 0);
	pcibx_write(dev, PCIBX_REG_RST_2, 0);
}
