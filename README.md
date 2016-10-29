#Build Notes:

##Mac
~~~~
g++ … -L/opt/local/lib -flat_namespace -undefined suppress -shared
~~~~

##PC/Cygwin
The following may be added to the pcb build command to generate the needed
import listing: 
~~~~
-Wl,--out-implib=pcb.a
~~~~

A Cygwin command line build:

~~~~
g++ \
../stipple.cpp ../dialog.cpp ../pcb.a \
-shared -g3 -o test.so \
-DHAVE_CONFIG_H \
-I/usr/include \
-I/usr/include/atk-1.0 \
-I/usr/include/cairo \
-I/usr/include/pango-1.0 \
-I/usr/include/glib-2.0 \
-I/usr/include/gdk-pixbuf-2.0 \
-I/usr/include/gtk-2.0 \
-I/usr/lib/gtk-2.0/inclide \
-I/lib/gtk-2.0/include \
-I/lib/glib-2.0/include \
-I/usr/src/pcb-20110918/src \
-I/usr/src/pcb-20110918 \
-D_REENTRANT -I/usr/lib/gtk-2.0/include -I/usr/include/cairo \
-I/usr/include/gdk-pixbuf-2.0 -I/usr/include/gio-unix-2.0/ \
-I/usr/lib/glib-2.0/include -I/usr/include/pixman-1 \
-I/usr/include/freetype2 -I/usr/include/libpng15 \
-I/usr/include/harfbuzz  -lgtk-x11-2.0 -lgdk-x11-2.0 -latk-1.0 \
-lpangocairo-1.0 -lXinerama -lXi -lXrandr -lXcursor -lXcomposite \
-lXdamage -lgdk_pixbuf-2.0 -lpangoft2-1.0 -lgio-2.0 -lXfixes \
-lcairo -lpixman-1 -lxcb-shm -lxcb-render -lXrender -lXext \
-lX11 -lxcb -lXau -lXdmcp -lpng15 -lharfbuzz -lpango-1.0 -lm \
-lfontconfig -lexpat -lfreetype -lz -lbz2 -lgmodule-2.0 \
-lgobject-2.0 -lffi -lglib-2.0 -lintl -liconv -lpcre
~~~~
