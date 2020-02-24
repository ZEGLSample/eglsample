#include "init_kms.h"

int init_kms(int fd, struct kms* kms)
{
   drmModeRes *resources;
   drmModeConnector *connector;
   drmModeEncoder *encoder;
   int i;

   resources = drmModeGetResources(fd);
   if (!resources) {
      fprintf(stderr, "drmModeGetResources failed\n");
      return 0;
   }

   for (i = 0; i < resources->count_connectors; i++) {
      connector = drmModeGetConnector(fd, resources->connectors[i]);
      if (connector == NULL)
	 	continue;

      if (connector->connection == DRM_MODE_CONNECTED &&
	  	connector->count_modes > 0)
	 	break;

      drmModeFreeConnector(connector);
   }

   if (i == resources->count_connectors) {
      fprintf(stderr, "No currently active connector found.\n");
      return 0;
   }

   for (i = 0; i < resources->count_encoders; i++) {
      encoder = drmModeGetEncoder(fd, resources->encoders[i]);

      if (encoder == NULL)
	 	continue;

      if (encoder->encoder_id == connector->encoder_id)
	 	break;

      drmModeFreeEncoder(encoder);
   }
	
   if(encoder && encoder->crtc_id)
	   kms->crtc = drmModeGetCrtc(fd, encoder->crtc_id);

   kms->res = resources;
   kms->connector = connector;
   kms->encoder = encoder;
   kms->mode = connector->modes[0];

   return 1;
}
