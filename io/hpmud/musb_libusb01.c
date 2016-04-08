/*****************************************************************************\

  musb.c - USB support for multi-point transport driver

  (c) 2010 Copyright HP Development Company, LP

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
  of the Software, and to permit persons to whom the Software is furnished to do
  so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  Client/Server generic message format (see messaging-protocol.doc):

Author: Naga Samrat Chowdary Narla, Sarbeswar Meher
\*****************************************************************************/

#include "hpmud.h"
#include "hpmudi.h"
#include <dlfcn.h>
#include "utils.h"

mud_device_vf __attribute__ ((visibility ("hidden"))) musb_mud_device_vf =
{
    .read = musb_read,
    .write = musb_write,
    .open = musb_open,
    .close = musb_close,
    .get_device_id = musb_get_device_id,
    .get_device_status = musb_get_device_status,
    .channel_open = musb_channel_open,
    .channel_close = musb_channel_close,
    .channel_write = musb_channel_write,
    .channel_read = musb_channel_read
};

static mud_channel_vf musb_raw_channel_vf =
{
    .open = musb_raw_channel_open,
    .close = musb_raw_channel_close,
    .channel_write = musb_raw_channel_write,
    .channel_read = musb_raw_channel_read
};

static mud_channel_vf musb_comp_channel_vf =
{
    .open = musb_comp_channel_open,
    .close = musb_raw_channel_close,
    .channel_write = musb_raw_channel_write,
    .channel_read = musb_raw_channel_read
};

static mud_channel_vf musb_mlc_channel_vf =
{
    .open = musb_mlc_channel_open,
    .close = musb_mlc_channel_close,
    .channel_write = musb_mlc_channel_write,
    .channel_read = musb_mlc_channel_read
};

static mud_channel_vf musb_dot4_channel_vf =
{
    .open = musb_dot4_channel_open,
    .close = musb_dot4_channel_close,
    .channel_write = musb_dot4_channel_write,
    .channel_read = musb_dot4_channel_read
};

/*
 * The folloing fd arrays must match "enum FD_ID" definition.
 */

static char *fd_name[MAX_FD] =
{
    "na",
    "7/1/2",
    "7/1/3",
    "7/1/4",
    "ff/1/1",
    "ff/2/1",
    "ff/3/1",
    "ff/ff/ff",
    "ff/d4/0",
    "ff/4/1",
    "ff/1/0",
    "ff/cc/0",
    "ff/2/10",
    "ff/9/1",
};

static int fd_class[MAX_FD] =
{
    0,0x7,0x7,0x7,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
};

static int fd_subclass[MAX_FD] =
{
    0,0x1,0x1,0x1,0x1,0x2,0x3,0xff,0xd4,0x4,0x1,0xcc,0x2,0x9,
};

static int fd_protocol[MAX_FD] =
{
    0,0x2,0x3,0x4,0x1,0x1,0x1,0xff,0,0x1,0,0,0x10,0x1,
};

static const unsigned char venice_power_on[] = {0x1b, '%','P','u','i','f','p','.','p','o','w','e','r',' ','1',';',
    'u','d','w','.','q','u','i','t',';',0x1b,'%','-','1','2','3','4','5','X' };

static struct usb_device *libusb_device;       /* libusb device referenced by URI */
//static int open_fd;                            /* 7/1/2 file descriptor, used by deviceid and status */
static file_descriptor fd_table[MAX_FD];       /* usb file descriptors */

/* This function is similar to usb_get_string_simple, but it handles zero returns. */
static int get_string_descriptor(usb_dev_handle *dev, int index, char *buf, size_t buflen)
{
    char tbuf[255];       /* Some devices choke on size > 255 */
    int ret, si, di, cnt=5;

    while (cnt--)
    {
        ret = usb_control_msg(dev, USB_ENDPOINT_IN, USB_REQ_GET_DESCRIPTOR, (USB_DT_STRING << 8) + index,
                0x409, tbuf, sizeof(tbuf), LIBUSB_CONTROL_REQ_TIMEOUT);
        if (ret==0)
        {
            /* This retry is necessary for lj1000 and lj1005. des 12/12/07 */
            BUG("get_string_descriptor zero result, retrying...");
            continue;
        }
        break;
    }

    if (ret < 0)
    {
        BUG("unable get_string_descriptor %d: %m\n", ret);
        return ret;
    }

    if (tbuf[1] != USB_DT_STRING)
    {
        BUG("invalid get_string_descriptor tag act=%d exp=%d\n", tbuf[1], USB_DT_STRING);
        return -EIO;
    }

    if (tbuf[0] > ret)
    {
        BUG("invalid get_string_descriptor size act=%d exp=%d\n", tbuf[0], ret);
        return -EFBIG;
    }

    for (di = 0, si = 2; si < tbuf[0]; si += 2)
    {
        if (di >= (buflen - 1))
            break;

        if (tbuf[si + 1])   /* high byte */
            buf[di++] = '0';
        else
            buf[di++] = tbuf[si];
    }

    buf[di] = 0;

    return di;
}

/* Check for USB interface descriptor with specified class. */
static int is_interface(struct usb_device *dev, int dclass)
{
    struct usb_interface_descriptor *pi;
    int i, j, k;

    for (i=0; i<dev->descriptor.bNumConfigurations; i++)
    {
        for (j=0; j<dev->config[i].bNumInterfaces; j++)
        {
            for (k=0; k<dev->config[i].interface[j].num_altsetting; k++)
            {
                pi = &dev->config[i].interface[j].altsetting[k];
                if (pi->bInterfaceClass == dclass)
                {
                    return 1;            /* found interface */
                }
            }
        }
    }
    return 0;    /* no interface found */
}


/* Check for USB interface descriptor with print interface. */
static int is_printer(struct usb_device *dev)
{
    struct usb_interface_descriptor *pi;
    int i, j, k;

    for (i=0; i<dev->descriptor.bNumConfigurations; i++)
    {
        for (j=0; j<dev->config[i].bNumInterfaces; j++)
        {
            for (k=0; k<dev->config[i].interface[j].num_altsetting; k++)
            {
                pi = &dev->config[i].interface[j].altsetting[k];
                if (pi->bInterfaceClass == 0x07 && pi->bInterfaceProtocol == 0x02)
                {
                    return 1;            /* found interface */
                }
            }
        }
    }
    return 0;    /* no interface found */
}

/* Write HP vendor-specific ECP channel message. */
static int write_ecp_channel(file_descriptor *pfd, int value)
{
    usb_dev_handle *hd;
    int interface = pfd->interface;
    int len, stat=1;
    char byte;

    if (pfd->hd == NULL)
    {
        BUG("invalid write_ecp_channel state\n");
        goto bugout;
    }

    hd = pfd->hd;

    len = usb_control_msg(hd,
            USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_INTERFACE, /* bmRequestType */
            USB_REQ_GET_STATUS,        /* bRequest */
            value,        /* wValue */
            interface, /* wIndex */
            &byte, 1, LIBUSB_CONTROL_REQ_TIMEOUT);

    if (len != 1)
    {
        BUG("invalid write_ecp_channel: %m\n");
        goto bugout;
    }

    stat = 0;

bugout:
    return stat;
}

/* Set Cypress USS-725 Bridge Chip to 1284.4 mode. */
static int bridge_chip_up(file_descriptor *pfd)
{
    usb_dev_handle *hd;
    int len, stat=1;
    char buf[9];
    char nullByte=0;

    if (pfd->hd == NULL)
    {
        BUG("invalid bridge_chip_up state\n");
        goto bugout;
    }

    hd = pfd->hd;

    memset(buf, 0, sizeof(buf));

    /* Read register values. */
    len = usb_control_msg(hd,
            USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE, /* bmRequestType */
            USB_REQ_SET_FEATURE,        /* bRequest */
            0,        /* wValue */
            0, /* wIndex */
            buf, sizeof(buf), LIBUSB_CONTROL_REQ_TIMEOUT);
    if (len < 0)
    {
        BUG("invalid write_bridge_up: %m\n");
        goto bugout;
    }

    /* Check for auto ECP mode. */
    if (buf[ECRR] != 0x43)
    {
        /* Place 725 chip in register mode. */
        len = usb_control_msg(hd,
                USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE, /* bmRequestType */
                0x04,        /* bRequest */
                0x0758,        /* wValue */
                0, /* wIndex */
                NULL, 0, LIBUSB_CONTROL_REQ_TIMEOUT);
        /* Turn off RLE in auto ECP mode. */
        len = usb_control_msg(hd,
                USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE, /* bmRequestType */
                0x04,        /* bRequest */
                0x0a1d,        /* wValue */
                0, /* wIndex */
                NULL, 0, LIBUSB_CONTROL_REQ_TIMEOUT);
        /* Place 725 chip in auto ECP mode. */
        len = usb_control_msg(hd,
                USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE, /* bmRequestType */
                0x04,        /* bRequest */
                0x0759,        /* wValue */
                0, /* wIndex */
                NULL, 0, LIBUSB_CONTROL_REQ_TIMEOUT);
        /* Force negotiation. */
        len = usb_control_msg(hd,
                USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE, /* bmRequestType */
                0x04,        /* bRequest */
                0x0817,        /* wValue */
                0, /* wIndex */
                NULL, 0, LIBUSB_CONTROL_REQ_TIMEOUT);
        /* Read register values. */
        len = usb_control_msg(hd,
                USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE, /* bmRequestType */
                USB_REQ_SET_FEATURE,        /* bRequest */
                0,        /* wValue */
                0, /* wIndex */
                buf, sizeof(buf), LIBUSB_CONTROL_REQ_TIMEOUT);
        if (buf[ECRR] != 0x43)
        {
            BUG("invalid auto ecp mode mode=%d\n", buf[ECRR]);
        }
    }

    /* Reset to ECP channel 0. */
    len = usb_control_msg(hd,
            USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE, /* bmRequestType */
            0x04,        /* bRequest */
            0x05ce,        /* wValue */
            0, /* wIndex */
            NULL, 0, LIBUSB_CONTROL_REQ_TIMEOUT);
    musb_write(pfd->fd, &nullByte, 1, HPMUD_EXCEPTION_TIMEOUT);

    /* Switch to ECP channel 77. */
    len = usb_control_msg(hd,
            USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE, /* bmRequestType */
            0x04,        /* bRequest */
            0x05cd,        /* wValue */
            0, /* wIndex */
            NULL, 0, LIBUSB_CONTROL_REQ_TIMEOUT);

    stat = 0;

bugout:
    return stat;
}

/* Set Cypress USS-725 Bridge Chip to compatibility mode. */
static int bridge_chip_down(file_descriptor *pfd)
{
    usb_dev_handle *hd;
    int len, stat=1;

    if (pfd->hd == NULL)
    {
        BUG("invalid bridge_chip_down state\n");
        goto bugout;
    }

    hd = pfd->hd;

    len = usb_control_msg(hd,
            USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE, /* bmRequestType */
            0x04,        /* bRequest */
            0x080f,        /* wValue */
            0, /* wIndex */
            NULL, 0, LIBUSB_CONTROL_REQ_TIMEOUT);
    if (len < 0)
    {
        BUG("invalid write_bridge_up: %m\n");
        goto bugout;
    }

    stat = 0;

bugout:
    return stat;
}

/* Write HP vendor-specific Setup command. */
static int write_phoenix_setup(file_descriptor *pfd)
{
    usb_dev_handle *hd;
    int len, stat=1;

    if (pfd->hd == NULL)
    {
        BUG("invalid write_phoenix_setup state\n");
        goto bugout;
    }

    hd = pfd->hd;

    len = usb_control_msg(hd,
            USB_ENDPOINT_OUT | USB_TYPE_CLASS | USB_RECIP_OTHER, /* bmRequestType */
            0x02,        /* bRequest */
            0,        /* wValue */
            0, /* wIndex */
            NULL, 0, LIBUSB_CONTROL_REQ_TIMEOUT);

    if (len < 0)
    {
        BUG("invalid write_phoenix_setup: %m\n");
        goto bugout;
    }

    stat = 0;

bugout:
    return stat;
}

/* Detach any kernel module that may have claimed specified inteface. */
static int detach(usb_dev_handle *hd, int interface)
{
    char driver[32];

    driver[0] = 0;

#ifdef LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP
    /* If any kernel module (ie:usblp) has claimed this interface, detach it. */
    usb_get_driver_np(hd, interface, driver, sizeof(driver));
    if ((driver[0] != 0) && (strcasecmp(driver, "usbfs") != 0))
    {
        DBG("removing %s driver interface=%d\n", driver, interface);
        if (usb_detach_kernel_driver_np(hd, interface) < 0)
            BUG("could not remove %s driver interface=%d: %m\n", driver, interface);
    }
#endif

    return 0;
}

/* Get interface descriptor for specified xx/xx/xx protocol. */
static int get_interface(struct usb_device *dev, enum FD_ID index, file_descriptor *pfd)
{
    struct usb_interface_descriptor *pi;
    int i, j, k;

    for (i=0; i<dev->descriptor.bNumConfigurations; i++)
    {
        if (dev->config == NULL)
            goto bugout;

        for (j=0; j<dev->config[i].bNumInterfaces; j++)
        {
            if (dev->config[i].interface == NULL)
                goto bugout;

            for (k=0; k<dev->config[i].interface[j].num_altsetting; k++)
            {
                if (dev->config[i].interface[j].altsetting == NULL)
                    goto bugout;

                pi = &dev->config[i].interface[j].altsetting[k];
                if (pi->bInterfaceClass == fd_class[index] && pi->bInterfaceSubClass == fd_subclass[index] && pi->bInterfaceProtocol == fd_protocol[index])
                {
                    pfd->config=i;            /* found interface */
                    pfd->interface=j;
                    pfd->alt_setting=k;
                    pfd->fd=index;
                    return 0;
                }
            }
        }
    }

bugout:
    return 1;    /* no interface found */
}

/* Get out endpoint for specified interface descriptor. */
static int get_out_ep(struct usb_device *dev, int config, int interface, int altset, int type)
{
    struct usb_interface_descriptor *pi;
    int i;

    if (dev->config == NULL || dev->config[config].interface == NULL || dev->config[config].interface[interface].altsetting == NULL)
        goto bugout;

    pi = &dev->config[config].interface[interface].altsetting[altset];
    for (i=0; i<pi->bNumEndpoints; i++)
    {
        if (pi->endpoint == NULL)
            goto bugout;
        if (pi->endpoint[i].bmAttributes == type && !(pi->endpoint[i].bEndpointAddress & USB_ENDPOINT_DIR_MASK)) {
            DBG("get_out_ep(type=%d): out=%d\n", type, pi->endpoint[i].bEndpointAddress);
            return pi->endpoint[i].bEndpointAddress;
        }
    }

bugout:
    DBG("get_out_ep: ERROR! returning -1\n");
    return -1; /* no endpoint found */
}

/* Get in endpoint for specified interface descriptor. */
static int get_in_ep(struct usb_device *dev, int config, int interface, int altset, int type)
{
    struct usb_interface_descriptor *pi;
    int i;

    if (dev->config == NULL || dev->config[config].interface == NULL || dev->config[config].interface[interface].altsetting == NULL)
        goto bugout;

    pi = &dev->config[config].interface[interface].altsetting[altset];
    for (i=0; i<pi->bNumEndpoints; i++)
    {
        if (pi->endpoint == NULL)
            goto bugout;
        if (pi->endpoint[i].bmAttributes == type && (pi->endpoint[i].bEndpointAddress & USB_ENDPOINT_DIR_MASK)) {
            DBG("get_in_ep(type=%d): out=%d\n", type, pi->endpoint[i].bEndpointAddress);
            return pi->endpoint[i].bEndpointAddress;
        }
    }

bugout:
    DBG("get_in_ep: ERROR! returning -1\n");
    return -1;  /* no endpoint found */
}

static int claim_interface(struct usb_device *dev, file_descriptor *pfd)
{
    int stat=1;

    if (pfd->hd != NULL)
        return 0;  /* interface is already claimed */

    if ((pfd->hd = usb_open(dev)) == NULL)
    {
        BUG("invalid usb_open: %m\n");
        goto bugout;
    }

    detach(pfd->hd, pfd->interface);

#if 0   /* hp devices only have one configuration, so far ... */
    if (usb_set_configuration(FD[fd].pHD, dev->config[config].bConfigurationValue))
        goto bugout;
#endif

    if (usb_claim_interface(pfd->hd, pfd->interface))
    {
        usb_close(pfd->hd);
        pfd->hd = NULL;
        DBG("invalid claim_interface %s: %m\n", fd_name[pfd->fd]);
        goto bugout;
    }

    if (usb_set_altinterface(pfd->hd, pfd->alt_setting))
    {
        usb_release_interface(pfd->hd, pfd->interface);
        usb_close(pfd->hd);
        pfd->hd = NULL;
        BUG("invalid set_altinterface %s altset=%d: %m\n", fd_name[pfd->fd], pfd->alt_setting);
        goto bugout;
    }

    pfd->write_active=0;
    pthread_mutex_init(&pfd->mutex, NULL);
    pthread_cond_init(&pfd->write_done_cond, NULL);

    DBG("claimed %s interface\n", fd_name[pfd->fd]);

    stat=0;

bugout:
    return stat;
}

static int release_interface(file_descriptor *pfd)
{
    if (pfd->hd == NULL)
        return 0;

    if (pfd->write_active)
    {
        BUG("aborting outstanding %s write\n", fd_name[pfd->fd]);
        pthread_cancel(pfd->tid);    /* kill outstanding write */
        pfd->write_active = 0;
    }

    usb_release_interface(pfd->hd, pfd->interface);
    usb_close(pfd->hd);
    pfd->hd = NULL;
    pthread_mutex_destroy(&pfd->mutex);
    pthread_cond_destroy(&pfd->write_done_cond);

    DBG("released %s interface\n", fd_name[pfd->fd]);

    return 0;
}

/* Claim any open interface which is valid for device_id and device status. */
static int claim_id_interface(struct usb_device *dev)
{
    enum FD_ID i;

    for (i=FD_7_1_2; i!=MAX_FD; i++)
    {
        if (get_interface(dev, i, &fd_table[i]) == 0)
        {
            if (claim_interface(libusb_device, &fd_table[i]))
                continue;  /* interface is busy, try next interface */
            break;  /* done */
        }
    }

    return i;
}

/* See if this usb device and URI match. */
static int is_uri(struct usb_device *dev, const char *uri)
{
    usb_dev_handle *hd=NULL;
    char sz[128];
    char uriModel[128];
    char uriSerial[128];
    char gen[128];
    int r, stat=0;

    if ((hd = usb_open(dev)) == NULL)
    {
        BUG("invalid usb_open: %m\n");
        goto bugout;
    }

    if (dev->descriptor.idVendor != 0x3f0)
        goto bugout;

    if ((r=get_string_descriptor(hd, dev->descriptor.iProduct, sz, sizeof(sz))) < 0)
    {
        BUG("invalid product id string ret=%d\n", r);
        goto bugout;
    }

    generalize_model(sz, gen, sizeof(gen));

    hpmud_get_uri_model(uri, uriModel, sizeof(uriModel));
    if (strcasecmp(uriModel, gen) != 0)
        goto bugout;

    if ((r=get_string_descriptor(hd, dev->descriptor.iSerialNumber, sz, sizeof(sz))) < 0)
    {
        BUG("invalid serial id string ret=%d\n", r);
        goto bugout;
    }

    if (sz[0])
        generalize_serial(sz, gen, sizeof(gen));
    else
        strcpy(gen, "0");

    get_uri_serial(uri, uriSerial, sizeof(uriSerial));
    if (strcmp(uriSerial, gen) != 0)
        goto bugout;

    stat = 1;    /* found usb device that matches uri */

bugout:
    if (hd != NULL)
        usb_close(hd);

    return stat;
}

/* See if this usb device and serial number match. Return model if match. */
static int is_serial(struct usb_device *dev, const char *sn, char *model, int model_size)
{
    usb_dev_handle *hd=NULL;
    char sz[128];
    char gen[128];
    int r, stat=0;

    if ((hd = usb_open(dev)) == NULL)
    {
        BUG("invalid usb_open: %m\n");
        goto bugout;
    }

    if (dev->descriptor.idVendor != 0x3f0)
        goto bugout;      /* not a HP product */

    if ((r=get_string_descriptor(hd, dev->descriptor.iSerialNumber, sz, sizeof(sz))) < 0)
    {
        BUG("invalid serial id string ret=%d\n", r);
        goto bugout;
    }
    if (sz[0])
        generalize_serial(sz, gen, sizeof(gen));
    else
        strcpy(gen, "0");

    if (strncmp(sn, gen, sizeof(gen)) != 0)
        goto bugout;  /* match failed */

    if ((r=get_string_descriptor(hd, dev->descriptor.iProduct, sz, sizeof(sz))) < 0)
    {
        BUG("invalid product id string ret=%d\n", r);
        goto bugout;
    }
    generalize_model(sz, model, model_size);

    stat = 1;    /* found usb device that matches sn */

bugout:
    if (hd != NULL)
        usb_close(hd);

    return stat;
}

static struct usb_device *get_libusb_device(const char *uri)
{
    struct usb_bus *bus;
    struct usb_device *dev;

    for (bus=usb_busses; bus; bus=bus->next)
        for (dev=bus->devices; dev; dev=dev->next)
            if (dev->descriptor.idVendor == 0x3f0 && is_interface(dev, 7))
                if (is_uri(dev, uri))
                    return dev;  /* found usb device that matches uri */

    return NULL;
}

static int device_id(int fd, char *buffer, int size)
{
    usb_dev_handle *hd;
    int config,interface,alt;
    int len=0, rlen, maxSize;

    hd = fd_table[fd].hd;
    config = fd_table[fd].config;
    interface = fd_table[fd].interface;
    alt = fd_table[fd].alt_setting;

    if (hd == NULL)
    {
        BUG("invalid device_id state\n");
        goto bugout;
    }

    maxSize = (size > 1024) ? 1024 : size;   /* RH8 has a size limit for device id (usb) */

    rlen = usb_control_msg(hd,
            USB_ENDPOINT_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE, /* bmRequestType */
            USB_REQ_GET_STATUS,        /* bRequest */
            config,        /* wValue */
            interface, /* wIndex */    /* note firmware does not follow the USB Printer Class specification for wIndex */
            buffer, maxSize, LIBUSB_CONTROL_REQ_TIMEOUT);

    if (rlen < 0)
    {
#if 0  /* Removed this PS A420 hack so a valid error is returned after USB reset. DES 10/1/09 */
        /* Following retry is necessary for a firmware problem with PS A420 products. DES 4/17/07 */
        BUG("invalid deviceid wIndex=%x, retrying wIndex=%x: %m\n", interface, interface << 8);
        rlen = usb_control_msg(hd,
                USB_ENDPOINT_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE, /* bmRequestType */
                USB_REQ_GET_STATUS,        /* bRequest */
                config,        /* wValue */
                interface << 8, /* wIndex */
                buffer, maxSize, LIBUSB_CONTROL_REQ_TIMEOUT);
        if (rlen < 0)
        {
            BUG("invalid deviceid retry ret=%d: %m\n", rlen);
            goto bugout;
        }
#endif
        BUG("invalid deviceid ret=%d: %m\n", rlen);
        goto bugout;
    }

    len = ntohs(*(short *)buffer);
    if (len > (size-1))
        len = size-1;   /* leave byte for zero termination */
    if (len > 2)
        len -= 2;
    memcpy(buffer, buffer+2, len);    /* remove length */
    buffer[len]=0;
    DBG("read actual device_id successfully fd=%d len=%d\n", fd, len);

bugout:
    return len; /* length does not include zero termination */
}

static int device_status(int fd, unsigned int *status)
{
    usb_dev_handle *hd;
    int interface;
    int len, stat=1;
    char byte;

    hd = fd_table[fd].hd;
    interface = fd_table[fd].interface;

    if (hd == NULL)
    {
        BUG("invalid device_status state\n");
        goto bugout;
    }

    len = usb_control_msg(hd,
            USB_ENDPOINT_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE, /* bmRequestType */
            USB_REQ_CLEAR_FEATURE,        /* bRequest */
            0,        /* wValue */
            interface, /* wIndex */
            &byte, 1, LIBUSB_CONTROL_REQ_TIMEOUT);

    if (len < 0)
    {
        BUG("invalid device_status: %m\n");
        goto bugout;
    }

    *status = (unsigned int)byte;
    stat = 0;
    DBG("read actual device_status successfully fd=%d\n", fd);

bugout:
    return stat;
}

/* Get VStatus from S-field. */
static int sfield_printer_state(const char *id)
{
    char *pSf;
    int vstatus=0, ver;

    if ((pSf = strstr(id, ";S:")) == NULL)
    {
        BUG("invalid S-field\n");
        return vstatus;
    }

    /* Valid S-field, get version number. */
    pSf+=3;
    ver = 0;
    HEX2INT(*pSf, ver);
    pSf++;
    ver = ver << 4;
    HEX2INT(*pSf, ver);
    pSf++;

    /* Position pointer to printer state subfield. */
    switch (ver)
    {
        case 0:
        case 1:
        case 2:
            pSf+=12;
            break;
        case 3:
            pSf+=14;
            break;
        case 4:
            pSf+=18;
            break;
        default:
            BUG("unknown S-field version=%d\n", ver);
            pSf+=12;
            break;
    }

    /* Extract VStatus.*/
    vstatus = 0;
    HEX2INT(*pSf, vstatus);
    pSf++;
    vstatus = vstatus << 4;
    HEX2INT(*pSf, vstatus);

    return vstatus;
}

/*
 * Power up printer if necessary. Most all-in-ones have no power down state (ie: OJ K80), so they are already powered up.
 * Newer single function printers power-up with the print job. May be called by other mud_device.
 */
int __attribute__ ((visibility ("hidden"))) power_up(mud_device *pd, int fd)
{
    const char *pSf;

    if ((pSf = strstr(pd->id, "CMD:LDL")) != NULL)
        return 0;   /* crossbow don't do power-up */

    if ((pSf = strstr(pd->id, ";S:")) != NULL)
    {
        if (sfield_printer_state(pd->id) != 3)
            return 0;     /* already powered up */
    }
    else if ((pSf = strstr(pd->id, "VSTATUS:")) != NULL)
    {
        /* DJ895C returns $XB0$XC0 (unknown pens) when powered off. */
        if (!(strstr(pSf+8, "OFFF") || strstr(pSf+8, "PWDN") || strstr(pSf+8, "$X")))
            return 0;   /* already powered up */
    }
    else
        return 0;  /* must be laserjet, don't do power-up */

    (pd->vf.write)(fd, venice_power_on, sizeof(venice_power_on), HPMUD_EXCEPTION_TIMEOUT);
    sleep(2);

    return 0;
}

/* Create channel object given the requested socket id and service name. */
static int new_channel(mud_device *pd, int index, const char *sn)
{
    int stat=1;

    /* Check for existing name service already open. */
    if (pd->channel[index].client_cnt)
    {
#if 0
        if (index == HPMUD_EWS_CHANNEL)
        {
            pd->channel[index].client_cnt++;  /* allow multiple clients for separate USB interfaces only */
            stat = 0;
            DBG("reused %s channel=%d clientCnt=%d channelCnt=%d\n", sn, index, pd->channel[index].client_cnt, pd->channel_cnt);
        }
        else
#endif
            BUG("%s channel=%d is busy, used by [%d], clientCnt=%d channelCnt=%d\n", sn, index, pd->channel[index].pid, pd->channel[index].client_cnt, pd->channel_cnt);
        goto bugout;
    }

    if (index == HPMUD_EWS_CHANNEL || index == HPMUD_EWS_LEDM_CHANNEL ||
            index == HPMUD_SOAPSCAN_CHANNEL || index == HPMUD_SOAPFAX_CHANNEL ||
            index == HPMUD_MARVELL_SCAN_CHANNEL || index == HPMUD_MARVELL_FAX_CHANNEL ||
            index == HPMUD_LEDM_SCAN_CHANNEL || index == HPMUD_MARVELL_EWS_CHANNEL ||
            index == HPMUD_IPP_CHANNEL || index == HPMUD_IPP_CHANNEL2 || index == HPMUD_ESCL_SCAN_CHANNEL) {
        pd->channel[index].vf = musb_comp_channel_vf;
    }
    else if (pd->io_mode == HPMUD_RAW_MODE || pd->io_mode == HPMUD_UNI_MODE) {
        pd->channel[index].vf = musb_raw_channel_vf;
    }
    else if (pd->io_mode == HPMUD_MLC_GUSHER_MODE || pd->io_mode == HPMUD_MLC_MISER_MODE) {
        pd->channel[index].vf = musb_mlc_channel_vf;
    }
    else {
        pd->channel[index].vf = musb_dot4_channel_vf;
    }

    pd->channel[index].index = index;
    pd->channel[index].client_cnt = 1;
    pd->channel[index].sockid = index;   /* static socket id is valid for MLC but not 1284.4 */
    pd->channel[index].pid = getpid();
    pd->channel[index].dindex = pd->index;
    pd->channel[index].fd = 0;
    strcpy(pd->channel[index].sn, sn);
    pd->channel_cnt++;

    stat = 0;
    DBG("new %s channel=%d clientCnt=%d channelCnt=%d\n", sn, index, pd->channel[index].client_cnt, pd->channel_cnt);

bugout:
    return stat;
}

/* Remove channel object given the channel decriptor. */
static int del_channel(mud_device *pd, mud_channel *pc)
{
    pc->client_cnt--;

    if (pc->client_cnt <= 0)
    {
        pd->channel_cnt--;
    }
    DBG("removed %s channel=%d clientCnt=%d channelCnt=%d\n", pc->sn, pc->index, pc->client_cnt, pd->channel_cnt);
    return 0;
}

static void write_thread(file_descriptor *pfd)
{
    int ep;

    pthread_detach(pthread_self());

    if ((ep = get_out_ep(libusb_device, pfd->config, pfd->interface, pfd->alt_setting, USB_ENDPOINT_TYPE_BULK)) < 0)
    {
        BUG("invalid bulk out endpoint\n");
        pfd->write_return = -ENOTCONN;
        goto bugout;
    }

    /* Wait forever for write to complete (actually 72 hours in ms). */
    pfd->write_return = usb_bulk_write(pfd->hd, ep, (char *)pfd->write_buf, pfd->write_size, 72*3600*1000);

bugout:
    pthread_mutex_lock(&pfd->mutex);
    pfd->write_buf = NULL;
    pthread_cond_signal(&pfd->write_done_cond);   /* signal write is complete */
    pthread_mutex_unlock(&pfd->mutex);

    return;
}

/*********************************************************************************************************************************
 * USB mud_device functions.
 */

int __attribute__ ((visibility ("hidden"))) musb_write(int fd, const void *buf, int size, int usec)
{
    int len=-EIO;

    if (fd_table[fd].hd == NULL)
    {
        BUG("invalid musb_write state\n");
        goto bugout;
    }

#if 1
    struct timeval now;
    struct timespec timeout;
    int ret;
    /* If write is still active, probably OOP condition, don't kick off a new write. */
    if (!fd_table[fd].write_active)
    {
        fd_table[fd].write_active = 1;
        fd_table[fd].write_buf = buf;
        fd_table[fd].write_size = size;

        /* Create usb_bulk_write thread so we can use our own timeout. Otherwise we cannot handle OOP condition. */
        if (pthread_create(&fd_table[fd].tid, NULL, (void *(*)(void*))write_thread, (void *)&fd_table[fd]) != 0)
        {
            BUG("unable to creat write_thread: %m\n");
            goto bugout; /* bail */
        }
    }

    /* Wait for write to complete. */
    pthread_mutex_lock(&fd_table[fd].mutex);
    gettimeofday(&now, NULL);
    now.tv_usec += usec;
    now.tv_sec += now.tv_usec / 1000000;
    now.tv_usec %= 1000000;
    timeout.tv_sec = now.tv_sec;
    timeout.tv_nsec = now.tv_usec * 1000;
    ret = 0;
    while (fd_table[fd].write_buf && ret != ETIMEDOUT)
        {
            ret = pthread_cond_timedwait(&fd_table[fd].write_done_cond, &fd_table[fd].mutex, &timeout);
        }
    pthread_mutex_unlock(&fd_table[fd].mutex);

    if (ret == ETIMEDOUT)
    {
        len = -ETIMEDOUT;     /* write timeout, let client know */
        goto bugout;
    }

    fd_table[fd].write_active = 0;

    len = fd_table[fd].write_return;
#else
    int ep;
    if ((ep = get_out_ep(libusb_device, fd_table[fd].config, fd_table[fd].interface, fd_table[fd].alt_setting, USB_ENDPOINT_TYPE_BULK)) < 0)
    {
        BUG("invalid bulk out endpoint\n");
        goto bugout;
    }

    len = usb_bulk_write(fd_table[fd].hd, ep, (char *)buf, size, usec);
#endif

    if (len < 0)
    {
        BUG("bulk_write failed buf=%p size=%d len=%d: %m\n", buf, size, len);
        goto bugout;
    }

    DBG("write fd=%d len=%d size=%d usec=%d\n", fd, len, size, usec);
    DBG_DUMP(buf, len < 512 ? len : 512);

bugout:
    return len;
}

int __attribute__ ((visibility ("hidden"))) musb_read(int fd, void *buf, int size, int usec)
{
    struct timeval t1, t2;
    int total_usec, tmo_usec=usec;
    int len=-EIO, ep;

    if (fd_table[fd].hd == NULL)
    {
        BUG("invalid musb_read state\n");
        goto bugout;
    }

    gettimeofday (&t1, NULL);     /* get start time */

    if ((ep = get_in_ep(libusb_device, fd_table[fd].config, fd_table[fd].interface, fd_table[fd].alt_setting, USB_ENDPOINT_TYPE_BULK)) < 0)
    {
        BUG("invalid bulk in endpoint\n");
        goto bugout;
    }

    while (1)
    {
        len = usb_bulk_read(fd_table[fd].hd, ep, (char *)buf, size, tmo_usec/1000);

        if (len == -ETIMEDOUT)
            goto bugout;

        if (len < 0)
        {
            BUG("bulk_read failed: %m\n");
            goto bugout;
        }

        if (len == 0)
        {
            /* Bulk_read has a timeout, but bulk_read can return zero byte packet(s), so we must use our own timeout here. */
            gettimeofday(&t2, NULL);   /* get current time */

            total_usec = (t2.tv_sec - t1.tv_sec)*1000000;
            total_usec += (t2.tv_usec > t1.tv_usec) ? t2.tv_usec - t1.tv_usec : t1.tv_usec - t2.tv_usec;
            if (total_usec > usec)
            {
                len = -ETIMEDOUT;   /* timeout */
                goto bugout;
            }
            tmo_usec = usec - total_usec;    /* decrease timeout */
            continue;
        }

        break;
    }

    DBG("read fd=%d len=%d size=%d usec=%d\n", fd, len, size, usec);
    DBG_DUMP(buf, len < 32 ? len : 32);

bugout:
    return len;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_open(mud_device *pd)
{
    int len=0, fd=0;
    enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;

    usb_init();
    usb_find_busses();
    usb_find_devices();

    /* Find usb device for specified uri. */
    if ((libusb_device = get_libusb_device(pd->uri)) == NULL)
    {
        BUG("unable to open %s\n", pd->uri);
        goto bugout;
    }

    pthread_mutex_lock(&pd->mutex);

    if (pd->id[0] == 0)
    {
        /* First client. */

        if ((fd = claim_id_interface(libusb_device)) == MAX_FD)
        {
            stat = HPMUD_R_DEVICE_BUSY;
            goto blackout;
        }

        len = device_id(fd, pd->id, sizeof(pd->id));  /* get new copy and cache it  */

        if (len > 0 && is_hp(pd->id))
            power_up(pd, fd);

        release_interface(&fd_table[fd]);

        if (len == 0)
            goto blackout;

        pd->open_fd = fd;
    }

    stat = HPMUD_R_OK;

blackout:
    pthread_mutex_unlock(&pd->mutex);

bugout:
    return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_close(mud_device *pd)
{
    int i;
    enum HPMUD_RESULT stat = HPMUD_R_OK;

    pthread_mutex_lock(&pd->mutex);

    for (i=1; i<MAX_FD; i++)
    {
        if (fd_table[i].hd != NULL)
            release_interface(&fd_table[i]);
    }

    pd->id[0] = 0;

    pthread_mutex_unlock(&pd->mutex);

    return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_get_device_id(mud_device *pd, char *buf, int size, int *len)
{
    int i, fd=FD_NA;
    enum HPMUD_RESULT stat = HPMUD_R_DEVICE_BUSY;

    *len=0;

    pthread_mutex_lock(&pd->mutex);

    if (pd->io_mode == HPMUD_DOT4_BRIDGE_MODE || pd->io_mode == HPMUD_UNI_MODE)
    {
        *len = strlen(pd->id);  /* usb/parallel bridge chip, use cached copy */
    }
    else
    {
        /* See if any interface is already claimed. */
        for (i=1; i<MAX_FD; i++)
        {
            if (fd_table[i].hd != NULL)
            {
                fd = i;
                break;
            }
        }

        if (fd == FD_NA)
        {
            /* Device not in use. Claim interface, but release for other processes. */
            if ((fd = claim_id_interface(libusb_device)) != MAX_FD)
            {
                *len = device_id(fd, pd->id, sizeof(pd->id));  /* get new copy and cache it  */
                release_interface(&fd_table[fd]);
            }
            else
            {
                /* Device is in use by another process, return cache copy. */
                *len = strlen(pd->id);
            }
        }
        else
        {
            /* Device in use by current process, leave interface up. Other processes are blocked. */
            *len = device_id(fd, pd->id, sizeof(pd->id));  /* get new copy and cache it  */
        }
    }

    if (*len)
    {
        memcpy(buf, pd->id, *len > size ? size : *len);
        stat = HPMUD_R_OK;
    }

    pthread_mutex_unlock(&pd->mutex);
    return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_get_device_status(mud_device *pd, unsigned int *status)
{
    int i, fd=FD_NA;
    enum HPMUD_RESULT stat = HPMUD_R_DEVICE_BUSY;
    int r=1;

    pthread_mutex_lock(&pd->mutex);

    if (pd->io_mode == HPMUD_DOT4_BRIDGE_MODE || pd->io_mode == HPMUD_UNI_MODE)
        *status = NFAULT_BIT;   /* usb/parallel bridge chip, fake status */
    else
    {
        /* See if any interface is already claimed. */
        for (i=1; i<MAX_FD; i++)
        {
            if (fd_table[i].hd != NULL)
            {
                fd = i;
                break;
            }
        }

        if (fd == FD_NA)
        {
            /* Device not in use. Claim interface, but release for other processes. */
            if ((fd = claim_id_interface(libusb_device)) != MAX_FD)
            {
                r = device_status(fd, status);
                release_interface(&fd_table[fd]);
            }
        }
        else
        {
            /* Device in use by current process, leave interface up. Other processes are blocked. */
            r = device_status(fd, status);
        }
    }

    pthread_mutex_unlock(&pd->mutex);

    if (r != 0)
        goto bugout;

    stat = HPMUD_R_OK;

bugout:
    return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_channel_write(mud_device *pd, mud_channel *pc, const void *buf, int length, int sec_timeout, int *bytes_wrote)
{
    enum HPMUD_RESULT stat;

    pthread_mutex_lock(&pd->mutex);
    stat  = (pc->vf.channel_write)(pc, buf, length, sec_timeout, bytes_wrote);
    pthread_mutex_unlock(&pd->mutex);
    return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_channel_read(mud_device *pd, mud_channel *pc, void *buf, int length, int sec_timeout, int *bytes_read)
{
    enum HPMUD_RESULT stat;

    if (pd->io_mode == HPMUD_UNI_MODE)
    {
        stat = HPMUD_R_INVALID_STATE;
        BUG("invalid channel_read io_mode=%d\n", pd->io_mode);
        goto bugout;
    }

    pthread_mutex_lock(&pd->mutex);
    stat  = (pc->vf.channel_read)(pc, buf, length, sec_timeout, bytes_read);
    pthread_mutex_unlock(&pd->mutex);

bugout:
    return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_channel_open(mud_device *pd, const char *sn, HPMUD_CHANNEL *cd)
{
    int index;
    enum HPMUD_RESULT stat;

    /* Check for valid service requests. */
    if ((stat = service_to_channel(pd, sn, &index)) != HPMUD_R_OK)
        goto bugout;

    pthread_mutex_lock(&pd->mutex);

    if (new_channel(pd, index, sn))
    {
        stat = HPMUD_R_DEVICE_BUSY;
    }
    else
    {
        if ((stat = (pd->channel[index].vf.open)(&pd->channel[index])) != HPMUD_R_OK)  /* call transport specific open */
            del_channel(pd, &pd->channel[index]);   /* open failed, cleanup */
        else
            *cd = index;
    }

    pthread_mutex_unlock(&pd->mutex);

bugout:
    return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_channel_close(mud_device *pd, mud_channel *pc)
{
    enum HPMUD_RESULT stat = HPMUD_R_OK;

    pthread_mutex_lock(&pd->mutex);
    stat = (pc->vf.close)(pc);      /* call trasport specific close */
    del_channel(pd, pc);
    pthread_mutex_unlock(&pd->mutex);

    return stat;
}

/*******************************************************************************************************************************
 * USB raw_channel functions.
 */

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_raw_channel_open(mud_channel *pc)
{
    int fd = FD_7_1_2;
    enum HPMUD_RESULT stat = HPMUD_R_DEVICE_BUSY;

    get_interface(libusb_device, fd, &fd_table[fd]);

    if (claim_interface(libusb_device, &fd_table[fd]))
        goto bugout;

    pc->fd = fd;

    stat = HPMUD_R_OK;

bugout:
    return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_raw_channel_close(mud_channel *pc)
{
    int fd = pc->fd;

    // For New laserjet devices like Tsunami, end point was getting stall or halted, hence clearing it
    int ep = -1;
    if (( ep = get_in_ep(libusb_device, fd_table[fd].config, fd_table[fd].interface, fd_table[fd].alt_setting, USB_ENDPOINT_TYPE_BULK)) >= 0)
    {
        usb_clear_halt(fd_table[fd].hd,  ep);
    }

    if (( ep = get_out_ep(libusb_device, fd_table[fd].config, fd_table[fd].interface, fd_table[fd].alt_setting, USB_ENDPOINT_TYPE_BULK)) >= 0)
    {
        usb_clear_halt(fd_table[fd].hd,  ep);
    }

    release_interface(&fd_table[fd]);

    pc->fd = 0;

    return HPMUD_R_OK;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_raw_channel_write(mud_channel *pc, const void *buf, int length, int sec_timeout, int *bytes_wrote)
{
    int len, size, total=0;
    enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;

    *bytes_wrote=0;
    size = length;

    while (size > 0)
    {
        len = (msp->device[pc->dindex].vf.write)(pc->fd, buf+total, size, sec_timeout*1000000);
        if (len < 0)
        {
            if (len == -ETIMEDOUT)
            {
                stat = HPMUD_R_IO_TIMEOUT;
                if (sec_timeout >= HPMUD_EXCEPTION_SEC_TIMEOUT)
                    BUG("unable to write data %s: %d second io timeout\n", msp->device[pc->dindex].uri, sec_timeout);
            }
            else
                BUG("unable to write data %s (len = %d): %m\n", msp->device[pc->dindex].uri, len);
            goto bugout;
        }
        size-=len;
        total+=len;
        *bytes_wrote+=len;
    }

    stat = HPMUD_R_OK;

bugout:
    return stat;
}

/*
 * Channel_read() tries to read "length" bytes from the peripheral. The returned read count may be zero
 * (timeout, no data available), less than "length" or equal "length".
 */
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_raw_channel_read(mud_channel *pc, void *buf, int length, int sec_timeout, int *bytes_read)
{
    int len=0, usec;
    enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;

    *bytes_read = 0;

    if (sec_timeout==0)
        usec = 1000;       /* minmum timeout is 1ms for libusb 0.1.12, hangs forever with zero */
    else
        usec = sec_timeout*1000000;

    len = (msp->device[pc->dindex].vf.read)(pc->fd, buf, length, usec);
    if (len < 0)
    {
        if (len == -ETIMEDOUT)
        {
            stat = HPMUD_R_IO_TIMEOUT;
            if (sec_timeout >= HPMUD_EXCEPTION_SEC_TIMEOUT)
                BUG("unable to read data %s: %d second io timeout\n", msp->device[pc->dindex].uri, sec_timeout);
        }
        else
            BUG("unable to read data %s: %m\n", msp->device[pc->dindex].uri);
        goto bugout;
    }

    *bytes_read = len;
    stat = HPMUD_R_OK;

bugout:
    return stat;
}

/*******************************************************************************************************************************
 * USB comp_channel functions.
 */

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_comp_channel_open(mud_channel *pc)
{
    int fd;
    enum HPMUD_RESULT stat = HPMUD_R_DEVICE_BUSY;

    /* Get requested composite interface. */
    switch (pc->index)
    {
        case HPMUD_EWS_CHANNEL:
            fd = FD_ff_1_1;
            break;
        case HPMUD_EWS_LEDM_CHANNEL:
            fd = FD_ff_4_1;
            break;
        case HPMUD_SOAPSCAN_CHANNEL:
            fd = FD_ff_2_1;
            break;
        case HPMUD_SOAPFAX_CHANNEL:
            fd = FD_ff_3_1;
            break;
        case HPMUD_MARVELL_SCAN_CHANNEL:
            fd = FD_ff_ff_ff;
            break;
        case HPMUD_MARVELL_FAX_CHANNEL:  //using vendor specific C/S/P codes for fax too
            fd = FD_ff_1_0;
            break;
        case HPMUD_LEDM_SCAN_CHANNEL:  //using vendor specific C/S/P codes for fax too
        case HPMUD_ESCL_SCAN_CHANNEL:
            fd = FD_ff_cc_0;
            break;
        case HPMUD_MARVELL_EWS_CHANNEL:
            fd = FD_ff_2_10;
            break;
        case HPMUD_IPP_CHANNEL:
            fd = FD_7_1_4;
            break;
        case HPMUD_IPP_CHANNEL2:
            fd = FD_ff_9_1;
            break;
        default:
            stat = HPMUD_R_INVALID_SN;
            BUG("invalid %s channel=%d\n", pc->sn, pc->index);
            goto bugout;
            break;
    }

    if (get_interface(libusb_device, fd, &fd_table[fd]))
    {
        stat = HPMUD_R_INVALID_SN;
        BUG("invalid %s channel=%d\n", pc->sn, pc->index);
        goto bugout;
    }

    if (claim_interface(libusb_device, &fd_table[fd]))
        goto bugout;

    pc->fd = fd;

    stat = HPMUD_R_OK;

bugout:
    return stat;
}

/*******************************************************************************************************************************
 * USB mlc_channel functions.
 */

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_mlc_channel_open(mud_channel *pc)
{
    mud_device *pd = &msp->device[pc->dindex];
    enum FD_ID fd;
    enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;

    /* Initialize MLC transport if this is the first MLC channel. */
    if (pd->channel_cnt==1)
    {
        if (get_interface(libusb_device, FD_7_1_3, &fd_table[FD_7_1_3]) == 0 && claim_interface(libusb_device, &fd_table[FD_7_1_3]) == 0)
            fd = FD_7_1_3;    /* mlc, dot4 */
        else if (get_interface(libusb_device, FD_ff_ff_ff, &fd_table[FD_ff_ff_ff]) == 0 && claim_interface(libusb_device, &fd_table[FD_ff_ff_ff]) == 0)
            fd = FD_ff_ff_ff;   /* mlc, dot4 */
        else if (get_interface(libusb_device, FD_ff_d4_0, &fd_table[FD_ff_d4_0]) == 0 && claim_interface(libusb_device, &fd_table[FD_ff_d4_0]) == 0)
            fd = FD_ff_d4_0;   /* mlc, dot4 */
        else if (get_interface(libusb_device, FD_7_1_2, &fd_table[FD_7_1_2]) == 0 && claim_interface(libusb_device, &fd_table[FD_7_1_2]) == 0)
            fd = FD_7_1_2;    /* raw, mlc, dot4 */
        else
        {
            stat = HPMUD_R_DEVICE_BUSY;
            goto bugout;
        }

        if (fd == FD_7_1_2)
        {
            /* Emulate 7/1/3 on 7/1/2 using vendor-specific ECP channel-77. */
            if (write_ecp_channel(&fd_table[fd], 77))
                goto bugout;
        }

        unsigned int i;
#if 0
        // Removed reverse drain I seen it hang forever on read, one-time with PSC750 (FC5 64-bit). DES
        int len;
        unsigned char buf[255];

        /* Drain any reverse data. */
        for (i=0,len=1; len > 0 && i < sizeof(buf); i++)
            len = (pd->vf.read)(fd, buf+i, 1, 0);    /* no blocking */
#endif

        /* MLC initialize */
        if (MlcInit(pc, fd) != 0)
            goto bugout;

        /* Reset transport attributes for all channels. */
        for (i=0; i<HPMUD_CHANNEL_MAX; i++)
            memset(&pd->channel[i].ta, 0 , sizeof(transport_attributes));

        pd->mlc_fd = fd;
        pd->mlc_up=1;

    } /* if (pDev->ChannelCnt==1) */

    if (MlcConfigSocket(pc, pd->mlc_fd))
        goto bugout;

    if (MlcOpenChannel(pc, pd->mlc_fd))
        goto bugout;

    pc->rcnt = pc->rindex = 0;

    stat = HPMUD_R_OK;

bugout:
    return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_mlc_channel_close(mud_channel *pc)
{
    mud_device *pd = &msp->device[pc->dindex];
    unsigned char nullByte=0;
    enum HPMUD_RESULT stat = HPMUD_R_OK;

    if (pd->mlc_up)
    {
        if (MlcCloseChannel(pc, pd->mlc_fd))
            stat = HPMUD_R_IO_ERROR;
    }

    /* Remove MLC transport if this is the last MLC channel. */
    if (pd->channel_cnt==1)
    {
        if (pd->mlc_up)
        {
            if (MlcExit(pc, pd->mlc_fd))
                stat = HPMUD_R_IO_ERROR;
        }
        pd->mlc_up=0;

        if (pd->mlc_fd == FD_7_1_2)
        {
            write_ecp_channel(&fd_table[pd->mlc_fd], 78);
            (pd->vf.write)(pd->mlc_fd, &nullByte, 1, HPMUD_EXCEPTION_TIMEOUT);
            write_ecp_channel(&fd_table[pd->mlc_fd], 0);
        }

        release_interface(&fd_table[pd->mlc_fd]);

        /* Delay for back-to-back scanning using scanimage (OJ 7110, OJ d135). */
        sleep(1);
    }

    return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_mlc_channel_write(mud_channel *pc, const void *buf, int length, int sec_timeout, int *bytes_wrote)
{
    mud_device *pd = &msp->device[pc->dindex];
    int ret, len, size, dlen, total=0;
    enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;

    *bytes_wrote=0;
    size = length;
    dlen = pc->ta.h2psize - sizeof(MLCHeader);
    while (size > 0)
    {
        len = (size > dlen) ? dlen : size;

        if (pc->ta.h2pcredit == 0 && pd->io_mode == HPMUD_MLC_MISER_MODE)
        {
            if (MlcCreditRequest(pc, pd->mlc_fd, 1))  /* Miser flow control */
            {
                BUG("invalid MlcCreditRequest from peripheral\n");
                goto bugout;
            }
        }

        if (pc->ta.h2pcredit == 0)
        {
            ret = MlcReverseCmd(pc, pd->mlc_fd);
            if (pc->ta.h2pcredit == 0)
            {
                if (ret == 0)
                    continue;  /* Got a reverse command, but no MlcCredit, try again. */

                if (pd->io_mode != HPMUD_MLC_MISER_MODE)
                {
                    /* If miser flow control works for this device, set "miser" in models.dat. */
                    BUG("invalid MlcCredit from peripheral, trying miser\n");
                    pd->io_mode = HPMUD_MLC_MISER_MODE;
                    continue;
                }

                BUG("invalid MlcCredit from peripheral\n");
                goto bugout;
            }
        }

        if (MlcForwardData(pc, pd->mlc_fd, buf+total, len, sec_timeout*1000000))
        {
            goto bugout;
        }

        pc->ta.h2pcredit--;
        size-=len;
        total+=len;
        *bytes_wrote+=len;
    }

    stat = HPMUD_R_OK;

bugout:
    return stat;
}

/*
 * Mlc_channel_read() tries to read "length" bytes from the peripheral. ReadData() reads data in packet size chunks.
 * The returned read count may be zero (timeout, no data available), less than "length" or equal "length".
 *
 * Mlc_channel_read() may read more the "length" if the data packet is greater than "length". For this case the
 * return value will equal "length" and the left over data will be buffered for the next ReadData() call.
 *
 * The "timeout" specifies how many seconds to wait for a data packet. Once the read of the data packet has
 * started the "timeout" is no longer used.
 *
 * Note, if a "timeout" occurs one peripheral to host credit is left outstanding. Which means the peripheral
 * can send unsolicited data later.
 */
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_mlc_channel_read(mud_channel *pc, void *buf, int length, int sec_timeout, int *bytes_read)
{
    mud_device *pd = &msp->device[pc->dindex];
    enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;

    *bytes_read=0;
    if (pc->ta.p2hsize==0)
    {
        BUG("invalid channel_read state\n");
        goto bugout;
    }

    if (pc->rcnt)
    {
        stat=HPMUD_R_OK;
        *bytes_read = cut_buf(pc, buf, length);
        goto bugout;
    }

    if (pc->ta.p2hcredit == 0)
    {
        /* Issue enough credit to the peripheral to read one data packet. */
        if (MlcCredit(pc, pd->mlc_fd, 1))
            goto bugout;
    }

    stat=HPMUD_R_OK;
    pc->rcnt = MlcReverseData(pc, pd->mlc_fd, pc->rbuf, sizeof(pc->rbuf), sec_timeout*1000000);
    if (pc->rcnt)
        pc->ta.p2hcredit--; /* one data packet was read, decrement credit count */

    *bytes_read = cut_buf(pc, buf, length);

bugout:
    return stat;
}

/*******************************************************************************************************************************
 * USB dot4_channel functions.
 */

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_dot4_channel_open(mud_channel *pc)
{
    mud_device *pd = &msp->device[pc->dindex];
    enum FD_ID fd;
    enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;

    /* Initialize MLC transport if this is the first MLC channel. */
    if (pd->channel_cnt==1)
    {
        if (get_interface(libusb_device, FD_7_1_3, &fd_table[FD_7_1_3]) == 0 && claim_interface(libusb_device, &fd_table[FD_7_1_3]) == 0)
            fd = FD_7_1_3;    /* mlc, dot4 */
        else if (get_interface(libusb_device, FD_ff_ff_ff, &fd_table[FD_ff_ff_ff]) == 0 && claim_interface(libusb_device, &fd_table[FD_ff_ff_ff]) == 0)
            fd = FD_ff_ff_ff;   /* mlc, dot4 */
        else if (get_interface(libusb_device, FD_ff_d4_0, &fd_table[FD_ff_d4_0]) == 0 && claim_interface(libusb_device, &fd_table[FD_ff_d4_0]) == 0)
            fd = FD_ff_d4_0;   /* mlc, dot4 */
        else if (get_interface(libusb_device, FD_7_1_2, &fd_table[FD_7_1_2]) == 0 && claim_interface(libusb_device, &fd_table[FD_7_1_2]) == 0)
            fd = FD_7_1_2;    /* raw, mlc, dot4 */
        else
        {
            stat = HPMUD_R_DEVICE_BUSY;
            goto bugout;
        }

        if (fd == FD_7_1_2)
        {
            if (pd->io_mode == HPMUD_DOT4_BRIDGE_MODE)
            {
                /* Emulate 7/1/3 on 7/1/2 using the bridge chip set (ie: CLJ2500). */
                if (bridge_chip_up(&fd_table[fd]))
                    goto bugout;
            }
            else
            {
                /* Emulate 7/1/3 on 7/1/2 using vendor-specific ECP channel-77. */
                if (write_ecp_channel(&fd_table[fd], 77))
                    goto bugout;
            }
        }

        if (pd->io_mode == HPMUD_DOT4_PHOENIX_MODE)
            write_phoenix_setup(&fd_table[fd]);

        unsigned int i;
#if 0
        //   Removed reverse drain LJ1015 can hang forever on read (FC5 64-bit). DES
        unsigned char buf[255];
        int len;

        /* Drain any reverse data. */
        for (i=0,len=1; len > 0 && i < sizeof(buf); i++)
            len = (pd->vf.read)(fd, buf+i, 1, 0);    /* no blocking */
#endif
        /* DOT4 initialize */
        if (Dot4Init(pc, fd) != 0)
            goto bugout;

        /* Reset transport attributes for all channels. */
        for (i=0; i<HPMUD_CHANNEL_MAX; i++)
            memset(&pd->channel[i].ta, 0 , sizeof(transport_attributes));

        pd->mlc_fd = fd;
        pd->mlc_up=1;

    } /* if (pDev->ChannelCnt==1) */

    if (Dot4GetSocket(pc, pd->mlc_fd))
        goto bugout;

    if (Dot4OpenChannel(pc, pd->mlc_fd))
        goto bugout;

    if (pd->io_mode == HPMUD_DOT4_PHOENIX_MODE)
    {
        /* Issue credit to peripheral. */
        if (Dot4Credit(pc, pd->mlc_fd, 2))
        {
            BUG("invalid Dot4Credit to peripheral\n");
            goto bugout;
        }
    }

    pc->rcnt = pc->rindex = 0;

    stat = HPMUD_R_OK;

bugout:
    return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_dot4_channel_close(mud_channel *pc)
{
    mud_device *pd = &msp->device[pc->dindex];
    enum HPMUD_RESULT stat = HPMUD_R_OK;

    if (pd->mlc_up)
    {
        if (Dot4CloseChannel(pc, pd->mlc_fd))
            stat = HPMUD_R_IO_ERROR;
    }

    /* Remove 1284.4 transport if this is the last 1284.4 channel. */
    if (pd->channel_cnt==1)
    {
        if (pd->mlc_up)
        {
            if (Dot4Exit(pc, pd->mlc_fd))
                stat = HPMUD_R_IO_ERROR;
        }
        pd->mlc_up=0;

        if (pd->mlc_fd == FD_7_1_2)
        {
            if (pd->io_mode == HPMUD_DOT4_BRIDGE_MODE)
            {
                bridge_chip_down(&fd_table[pd->mlc_fd]);
            }
            else
            {
                write_ecp_channel(&fd_table[pd->mlc_fd], 78);
                write_ecp_channel(&fd_table[pd->mlc_fd], 0);
            }
        }

        release_interface(&fd_table[pd->mlc_fd]);

        /* Delay for back-to-back scanning using scanimage (OJ 7110, OJ d135). */
        sleep(1);
    }

    return stat;
}

enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_dot4_channel_write(mud_channel *pc, const void *buf, int length, int sec_timeout, int *bytes_wrote)
{
    mud_device *pd = &msp->device[pc->dindex];
    int ret, len, size, dlen, total=0, cnt=0;
    enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;

    *bytes_wrote=0;
    size = length;
    dlen = pc->ta.h2psize - sizeof(DOT4Header);
    while (size > 0)
    {
        len = (size > dlen) ? dlen : size;

        if (pc->ta.h2pcredit == 0 && pd->io_mode == HPMUD_DOT4_PHOENIX_MODE)
        {
            /* Issue credit request to peripheral. */
            if (Dot4CreditRequest(pc, pd->mlc_fd, 1))
            {
                BUG("invalid Dot4CreditRequest from peripheral\n");
                goto bugout;
            }
            if (pc->ta.h2pcredit == 0)
            {
                if (cnt++ > HPMUD_EXCEPTION_SEC_TIMEOUT)
                {
                    BUG("invalid Dot4CreditRequest from peripheral\n");
                    goto bugout;
                }
                sleep(1);
                continue;    /* Got a valid Dot4CreditRequest but no credit from peripheral, try again. */
            }
        }

        if (pc->ta.h2pcredit == 0)
        {
            ret = Dot4ReverseCmd(pc, pd->mlc_fd);
            if (pc->ta.h2pcredit == 0)
            {
                if (ret == 0)
                    continue;  /* Got a reverse command, but no Dot4Credit, try again. */

                BUG("invalid Dot4Credit from peripheral\n");
                goto bugout;
            }
        }

        if (Dot4ForwardData(pc, pd->mlc_fd, buf+total, len, sec_timeout*1000000))
        {
            goto bugout;
        }

        pc->ta.h2pcredit--;
        size-=len;
        total+=len;
        *bytes_wrote+=len;
        cnt=0;
    }

    stat = HPMUD_R_OK;

bugout:
    return stat;
}

/*
 * dot4_read_data() tries to read "length" bytes from the peripheral. Read_data() reads data in packet size chunks.
 * The returned read count may be zero (timeout, no data available), less than "length" or equal "length".
 *
 * dot4_read_data() may read more the "length" if the data packet is greater than "length". For this case the
 * return value will equal "length" and the left over data will be buffered for the next read_data() call.
 *
 * The "timeout" specifies how many seconds to wait for a data packet. Once the read of the data packet has
 * started the "timeout" is no longer used.
 *
 * Note, if a "timeout" occurs one peripheral to host credit is left outstanding. Which means the peripheral
 * can send unsolicited data later.
 */
enum HPMUD_RESULT __attribute__ ((visibility ("hidden"))) musb_dot4_channel_read(mud_channel *pc, void *buf, int length, int sec_timeout, int *bytes_read)
{
    mud_device *pd = &msp->device[pc->dindex];
    enum HPMUD_RESULT stat = HPMUD_R_IO_ERROR;

    *bytes_read=0;
    if (pc->ta.p2hsize==0)
    {
        BUG("invalid channel_read state\n");
        goto bugout;
    }

    if (pc->rcnt)
    {
        stat=HPMUD_R_OK;
        *bytes_read = cut_buf(pc, buf, length);
        goto bugout;
    }

    if (pc->ta.p2hcredit == 0)
    {
        /* Issue enough credit to the peripheral to read one data packet. */
        if (Dot4Credit(pc, pd->mlc_fd, 1))
            goto bugout;
    }

    stat=HPMUD_R_OK;
    pc->rcnt = Dot4ReverseData(pc, pd->mlc_fd, pc->rbuf, sizeof(pc->rbuf), sec_timeout*1000000);
    if (pc->rcnt)
        pc->ta.p2hcredit--; /* one data packet was read, decrement credit count */

    *bytes_read = cut_buf(pc, buf, length);

bugout:
    return stat;
}

/*******************************************************************************************************************************
 * USB probe devices, walk the USB bus(s) looking for HP products.
 */

int __attribute__ ((visibility ("hidden"))) musb_probe_devices(char *lst, int lst_size, int *cnt, enum HPMUD_DEVICE_TYPE devtype)
{
    struct usb_bus *bus;
    struct usb_device *dev;
    usb_dev_handle *hd;
    struct hpmud_model_attributes ma;
    char rmodel[128];
    char rserial[128];
    char model[128];
    char serial[128];
    char mfg[128];
    char sz[HPMUD_LINE_SIZE];
    int r, size=0;

    usb_init();
    usb_find_busses();
    usb_find_devices();

    for (bus=usb_busses; bus; bus=bus->next)
    {
        for (dev=bus->devices; dev; dev=dev->next)
        {

            model[0] = serial[0] = rmodel[0] = rserial[0] = sz[0] = mfg[0] = 0;

            if (dev->descriptor.idVendor == 0x3f0 && is_interface(dev, 7))
            {
                if((devtype == HPMUD_PRINTER) && !is_printer(dev))
                {
                    continue; /*Skip all HP devices which do not have print interface*/
                }
                
                if((hd = usb_open(dev)) == NULL)
                {
                    BUG("Invalid usb_open: %m\n");
                    continue;
                }
                /* Found hp device. */
                if ((r=get_string_descriptor(hd, dev->descriptor.iProduct, rmodel, sizeof(rmodel))) < 0)
                    BUG("invalid product id string ret=%d\n", r);
                else
                    generalize_model(rmodel, model, sizeof(model));

                if ((r=get_string_descriptor(hd, dev->descriptor.iSerialNumber, rserial, sizeof(rserial))) < 0)
                    BUG("invalid serial id string ret=%d\n", r);
                else
                    generalize_serial(rserial, serial, sizeof(serial));

                if ((r=get_string_descriptor(hd, dev->descriptor.iManufacturer, sz, sizeof(sz))) < 0)
                    BUG("invalid manufacturer string ret=%d\n", r);
                else
                    generalize_serial(sz, mfg, sizeof(serial));

                if (!serial[0])
                    strcpy(serial, "0"); /* no serial number, make it zero */

                if (model[0])
                {
                    snprintf(sz, sizeof(sz), "hp:/usb/%s?serial=%s", model, serial);

                    /* See if device is supported by hplip. */
                    hpmud_query_model(sz, &ma);
                    if (ma.support != HPMUD_SUPPORT_TYPE_HPLIP)
                    {
                        BUG("ignoring %s support=%d\n", sz, ma.support);
                        continue;           /* ignor, not supported */
                    }

                    /*
                     * For Cups 1.2 we append a dummy deviceid. A valid deviceid would require us to claim the USB interface, thus removing usblp.
                     * This will allow us to do discovery and not disable other CUPS backend(s) who use /dev/usb/lpx instead of libusb.
                     */
                    if (strncasecmp(rmodel, "hp ", 3) == 0)
                        size += snprintf(lst+size, lst_size-size, "direct %s \"HP %s\" \"HP %s USB %s HPLIP\" \"MFG:%s;MDL:%s;CLS:PRINTER;DES:%s;SN:%s;\"\n",
                                sz, &rmodel[3], &rmodel[3], serial, mfg, rmodel, rmodel, rserial);
                    else
                        size += snprintf(lst+size, lst_size-size, "direct %s \"HP %s\" \"HP %s USB %s HPLIP\" \"MFG:%s;MDL:%s;CLS:PRINTER;DES:%s;SN:%s;\"\n",
                                sz, rmodel, rmodel, serial, mfg, rmodel, rmodel, rserial);

                    *cnt+=1;
                }
                usb_close(hd);
            }
        }
    }

    return size;
}

enum HPMUD_RESULT hpmud_make_usb_uri(const char *busnum, const char *devnum, char *uri, int uri_size, int *bytes_read)
{
    struct usb_bus *bus;
    struct usb_device *dev, *found_dev=NULL;
    usb_dev_handle *hd=NULL;
    char model[128];
    char serial[128];
    char sz[256];
    int r;
    enum HPMUD_RESULT stat = HPMUD_R_INVALID_DEVICE_NODE;

    DBG("[%d] hpmud_make_usb_uri() bus=%s dev=%s\n", getpid(), busnum, devnum);

    *bytes_read=0;

    usb_init();
    usb_find_busses();
    usb_find_devices();

    for (bus=usb_busses; bus && !found_dev; bus=bus->next)
        if (strcmp(bus->dirname, busnum) == 0)
            for (dev=bus->devices; dev && !found_dev; dev=dev->next)
                if (strcmp(dev->filename, devnum) == 0)
                    found_dev = dev;  /* found usb device that matches bus:device */

    if (found_dev == NULL)
    {
        BUG("invalid busnum:devnum %s:%s\n", busnum, devnum);
        goto bugout;
    }

    dev = found_dev;
    if ((hd = usb_open(dev)) == NULL)
    {
        BUG("invalid usb_open: %m\n");
        goto bugout;
    }

    model[0] = serial[0] = sz[0] = 0;

    if (dev->descriptor.idVendor == 0x3f0)
    {
        /* Found hp device. */
        if ((r=get_string_descriptor(hd, dev->descriptor.iProduct, sz, sizeof(sz))) < 0)
            BUG("invalid product id string ret=%d\n", r);
        else
            generalize_model(sz, model, sizeof(model));

        if ((r=get_string_descriptor(hd, dev->descriptor.iSerialNumber, sz, sizeof(sz))) < 0)
            BUG("invalid serial id string ret=%d\n", r);
        else
            generalize_serial(sz, serial, sizeof(serial));

        if (!serial[0])
            strcpy(serial, "0"); /* no serial number, make it zero */

        if( dev->config[0].bNumInterfaces == 1 && is_interface(dev, 8))
        {
             strcpy(serial, "SMART_INSTALL_ENABLED"); /* no serial number, make it zero */
        }
    }
    else
    {
        BUG("invalid vendor id: %d\n", dev->descriptor.idVendor);
        goto bugout;
    }

    if (!model[0] || !serial[0])
        goto bugout;

    *bytes_read = snprintf(uri, uri_size, "hp:/usb/%s?serial=%s", model, serial);
    stat = HPMUD_R_OK;

bugout:
    if (hd != NULL)
        usb_close(hd);

    return stat;
}

enum HPMUD_RESULT hpmud_make_usb_serial_uri(const char *sn, char *uri, int uri_size, int *bytes_read)
{
    struct usb_bus *bus;
    struct usb_device *dev, *found_dev=NULL;
    char model[128];
    enum HPMUD_RESULT stat = HPMUD_R_INVALID_DEVICE_NODE;

    DBG("[%d] hpmud_make_usb_serial_uri() sn=%s\n", getpid(), sn);

    *bytes_read=0;

    usb_init();
    usb_find_busses();
    usb_find_devices();

    for (bus=usb_busses; bus && !found_dev; bus=bus->next)
        for (dev=bus->devices; dev && !found_dev; dev=dev->next)
            if (is_serial(dev, sn, model, sizeof(model)))
                found_dev = dev;  /* found usb device that matches serial number */

    if (found_dev == NULL)
    {
        BUG("invalid sn %s\n", sn);
        goto bugout;
    }

    *bytes_read = snprintf(uri, uri_size, "hp:/usb/%s?serial=%s", model, sn);
    stat = HPMUD_R_OK;

bugout:
    return stat;
}

