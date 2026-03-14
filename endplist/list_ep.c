#include <stdio.h>
#include <stdlib.h>
#include <libusb.h>

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: list_ep <fd>\n"); return 1; }
    int fd = atoi(argv[argc-1]);
    fprintf(stderr, "fd=%d\n", fd);

    // グローバルオプションは init より前に設定
    libusb_set_option(NULL, LIBUSB_OPTION_WEAK_AUTHORITY);
    libusb_set_option(NULL, LIBUSB_OPTION_NO_DEVICE_DISCOVERY);

    libusb_context *ctx = NULL;
    int r = libusb_init(&ctx);
    fprintf(stderr, "init=%d ctx=%p\n", r, (void*)ctx);
    if (r < 0) { fprintf(stderr, "init failed\n"); return 1; }

    libusb_device_handle *handle = NULL;
    r = libusb_wrap_sys_device(ctx, (intptr_t)fd, &handle);
    fprintf(stderr, "wrap=%d handle=%p\n", r, (void*)handle);
    if (r < 0 || handle == NULL) return 1;

    libusb_device *dev = libusb_get_device(handle);
    fprintf(stderr, "dev=%p\n", (void*)dev);

    struct libusb_config_descriptor *cfg = NULL;
    r = libusb_get_config_descriptor(dev, 0, &cfg);
    fprintf(stderr, "get_config=%d\n", r);
    if (r < 0) return 1;

    for (int i = 0; i < cfg->bNumInterfaces; i++) {
        const struct libusb_interface *ifc = &cfg->interface[i];
        for (int j = 0; j < ifc->num_altsetting; j++) {
            const struct libusb_interface_descriptor *id = &ifc->altsetting[j];
            printf("ifc=%d class=0x%02x ep_count=%d\n", i, id->bInterfaceClass, id->bNumEndpoints);
            for (int k = 0; k < id->bNumEndpoints; k++) {
                printf("  ep=0x%02x attr=0x%02x\n",
                    id->endpoint[k].bEndpointAddress,
                    id->endpoint[k].bmAttributes);
            }
        }
    }
    libusb_free_config_descriptor(cfg);
    return 0;
}
