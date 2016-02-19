#include "ecore_drm2_private.h"

#ifndef DRM_CAP_TIMESTAMP_MONOTONIC
# define DRM_CAP_TIMESTAMP_MONOTONIC 0x6
#endif

#ifndef DRM_CAP_CURSOR_WIDTH
# define DRM_CAP_CURSOR_WIDTH 0x8
#endif

#ifndef DRM_CAP_CURSOR_HEIGHT
# define DRM_CAP_CURSOR_HEIGHT 0x9
#endif

EAPI const char *
ecore_drm2_device_find(const char *seat)
{
   Eina_List *devs, *l;
   const char *dev, *ret = NULL;
   Eina_Bool found = EINA_FALSE;
   Eina_Bool platform = EINA_FALSE;

   devs = eeze_udev_find_by_subsystem_sysname("drm", "card[0-9]*");
   if (!devs) return NULL;

   EINA_LIST_FOREACH(devs, l, dev)
     {
        const char *dpath;
        const char *dseat;
        const char *dparent;

        dpath = eeze_udev_syspath_get_devpath(dev);
        if (!dpath) continue;

        dseat = eeze_udev_syspath_get_property(dev, "ID_SEAT");
        if (!dseat) dseat = eina_stringshare_add("seat0");

        if ((seat) && (strcmp(seat, dseat)))
          goto cont;
        else if (strcmp(dseat, "seat0"))
          goto cont;

        dparent = eeze_udev_syspath_get_parent_filtered(dev, "pci", NULL);
        if (!dparent)
          {
             dparent =
               eeze_udev_syspath_get_parent_filtered(dev, "platform", NULL);
             platform = EINA_TRUE;
          }

        if (dparent)
          {
             if (!platform)
               {
                  const char *id;

                  id = eeze_udev_syspath_get_sysattr(dparent, "boot_vga");
                  if (id)
                    {
                       if (!strcmp(id, "1")) found = EINA_TRUE;
                       eina_stringshare_del(id);
                    }
               }
             else
               found = EINA_TRUE;

             eina_stringshare_del(dparent);
          }

cont:
        eina_stringshare_del(dpath);
        if (found) break;
     }

   if (!found) goto out;

   ret = eeze_udev_syspath_get_devpath(dev);

out:
   EINA_LIST_FREE(devs, dev)
     eina_stringshare_del(dev);

   return ret;
}

EAPI int
ecore_drm2_device_clock_id_get(int fd)
{
   uint64_t caps;
   int ret;

   EINA_SAFETY_ON_TRUE_RETURN_VAL((fd < 0), -1);

   ret = drmGetCap(fd, DRM_CAP_TIMESTAMP_MONOTONIC, &caps);
   if ((ret == 0) && (caps == 1))
     return CLOCK_MONOTONIC;
   else
     return CLOCK_REALTIME;
}

EAPI void
ecore_drm2_device_cursor_size_get(int fd, int *width, int *height)
{
   uint64_t caps;
   int ret;

   EINA_SAFETY_ON_TRUE_RETURN((fd < 0));

   if (width)
     {
        *width = 64;
        ret = drmGetCap(fd, DRM_CAP_CURSOR_WIDTH, &caps);
        if (ret == 0) *width = caps;
     }

   if (height)
     {
        *height = 64;
        ret = drmGetCap(fd, DRM_CAP_CURSOR_HEIGHT, &caps);
        if (ret == 0) *height = caps;
     }
}