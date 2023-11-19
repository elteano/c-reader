#include<stddef.h>
#include<stdint.h>
#include<notcurses/notcurses.h>

#include "std.h"
#include "logging.h"
#include "notcutils.h"

/*
 * Prints a popup box and allows user to type data. Returns the data typed by the user.
 *
 * nc_handle: handle to primary notcurses struct
 * dest: buffer to which to write data
 * len: size of the buffer
 * label: can be NULL; title to print on the prompt
 */
int getInput(struct notcurses * nc_handle, char * dest, size_t len, char * label)
{
  unsigned width, height;
  ncinput val;
  memset(&val, 0, sizeof(ncinput));

  struct ncplane * std_plane = notcurses_stddim_yx(nc_handle, &height, &width);
  ncplane_options * opts = malloc(sizeof(ncplane_options));
  memset(opts, 0, sizeof(ncplane_options));
  opts->y = height / 2 - 2;
  opts->x = width / 2 - 30;
  opts->name = "Input Popup";
  opts->rows = 4;
  opts->cols = 60;
  struct ncplane * popup_plane = ncplane_create(std_plane, opts);
  free(opts);

  unsigned rows = 0;
  unsigned cols = 0;
  ncplane_dim_yx(popup_plane, &rows, &cols);

  for (int c = 0; c < cols; ++c)
  {
    ncplane_putstr_yx(popup_plane, 0,       c, TOP_BORDER_CHAR);
    ncplane_putstr_yx(popup_plane, rows-1,  c, TOP_BORDER_CHAR);
  }
  for (int r = 0; r < rows; ++r)
  {
    ncplane_putstr_yx(popup_plane, r, 0,      SIDE_BORDER_CHAR);
    ncplane_putstr_yx(popup_plane, r, cols-1, SIDE_BORDER_CHAR);
  }
  ncplane_putstr_yx(popup_plane, 0,       0,      TL_CORNER_CHAR);
  ncplane_putstr_yx(popup_plane, 0,       cols-1, TR_CORNER_CHAR);
  ncplane_putstr_yx(popup_plane, rows-1,  0,      BL_CORNER_CHAR);
  ncplane_putstr_yx(popup_plane, rows-1,  cols-1, BR_CORNER_CHAR);
  if (label)
  {
    ncplane_putstr_yx(popup_plane, 1, 2, label);
  }
  ncplane_cursor_move_yx(popup_plane, 2, 2);
  ncplane_putchar(popup_plane, '>');
  notcurses_render(nc_handle);

  int cInd = 0;

  int cont = 1;
  do
  {
    notcurses_get_blocking(nc_handle, &val);
    if (val.evtype == NCTYPE_PRESS || val.evtype == NCTYPE_REPEAT)
    {
      if (val.id == NCKEY_ENTER)
      {
        cont = 0;
      }
      else
      {
        if (val.id == NCKEY_BACKSPACE)
        {
          if (cInd > 0)
          {
            unsigned xloc = ncplane_cursor_x(popup_plane);
            writelog_l(LOGLEVEL_DEBUG, "cursor at %u", xloc);
            ncplane_putchar_yx(popup_plane, -1, xloc-1, ' ');
            ncplane_cursor_move_yx(popup_plane, -1, xloc-1);
            --cInd;
          }
        }
        if (ncinput_ctrl_p(&val) && (val.id == 'c' || val.id == 'C'))
        {
          // Clear everything
          unsigned xloc = ncplane_cursor_x(popup_plane);
          unsigned xdest = xloc - cInd;
          ncplane_cursor_move_yx(popup_plane, -1, xdest);
          memset(dest, 0, len);
          for (; cInd > 0; --cInd)
          {
            ncplane_putchar(popup_plane, ' ');
          }
          ncplane_cursor_move_yx(popup_plane, -1, xdest);
        }
        else
        {
          if (dest && cInd < len - 1)
          {
            if (val.utf8[0] >= ' ' && val.utf8[0] < 127)
            {
              dest[cInd] = val.utf8[0];
              ncplane_putchar(popup_plane, dest[cInd]);
              ++cInd;
            }
          }
          else
          {
            cont = 0;
          }
        }
      }
    }
    notcurses_render(nc_handle);
  }
  while (cont);
  dest[cInd] = 0;

  notcurses_get_blocking(nc_handle, &val);
  ncplane_destroy(popup_plane);
  notcurses_render(nc_handle);

  return 0;
}

