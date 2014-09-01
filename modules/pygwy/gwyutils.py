import gwy

def save_dfield_to_png(container, datafield_name, filename, run_type):
   """
   Save desired datafield given by name stored in container to file.

   @param container: gwy.Container which has datafield of given name
   @param datafield_name: datafield name in string representation (like '/0/data')
   @param filename: expected filename
   @param run_type: select of interactive (RUN_INTERACTIVE) or noninteractive mode (RUN_NONINTERACTIVE)
   """
   gwy.gwy_app_data_browser_reset_visibility(container, gwy.VISIBILITY_RESET_SHOW_ALL)
   datafield_num = int(datafield_name.split('/')[1])
   gwy.gwy_app_data_browser_select_data_field(container, datafield_num)
   gwy.gwy_file_save(container, filename, run_type)

def get_data_fields(container):
   """
   Get list of available datafield stored in given container

   @param container: gwy.Container with datafields

   @return: a list of datafields
   """
   l = []
   for key in container.keys():
      x = container.get_value(key)
      if type(x) is gwy.DataField:
         l.append(x)

   return l

def get_data_fields_dir(container):
   """
   Get list of available datafield stored in given container as directory where key is key name and value is DataField object.

   @param container: gwy.Container with datafields

   @return: a directory of datafields
   """
   d = {}
   for key in container.keys_by_name():
      x = container.get_value_by_name(key)
      if type(x) is gwy.DataField:
         d[key] = x

   return d

def get_current_datafield():
   """
   Short version of function L{gwy.gwy_app_data_browser_get_current}(gwy.APP_DATA_FIELD)

   @return: current datafield
   """
   return gwy.gwy_app_data_browser_get_current(gwy.APP_DATA_FIELD)

def get_current_container():
   """
   Short version of function L{gwy.gwy_app_data_browser_get_current}(gwy.APP_CONTAINER)

   @return: current container
   """
   return gwy.gwy_app_data_browser_get_current(gwy.APP_CONTAINER)

#def gwy_datafield_get_stats(datafield):
#   d = {}
#   d["average"] = float()
#   d["ra"] = float()
#   d["rms"] = float()
#   d["skew"] = float()
#   d["kurtosis"] = float()
#   datafield.get_stats(d["average"], d["ra"], d["rms"], d["skew"], d["kurtosis"])
#   return d

try:
   import numpy as np
except ImportError:
   pass
else:
   def data_field_data_as_array(field):
      """Create a view the DataField's data as numpy array.

      The array can point to an invalid memory location after using other
      gwyddion functions and lead to application crash. Use with care.

      @param field: the L{gwy.DataField} to view
      @return: array viewing the data
      """
      class gwydfdatap():
         def __init__(self,addr,shape):
            data = (addr,False)
            stride = (8,shape[0]*8)
            self.__array_interface__= {
               'strides' : stride,
               'shape'   : shape,
               'data'    : data,
               'typestr' : "|f8",
               'version' : 3}

      addr = field.get_data_pointer()
      shape = (field.get_xres(),field.get_yres())
      return np.array(gwydfdatap(addr,shape),copy=False)

   def brick_data_as_array(brick):
      """Create a view the Brick's data as numpy array.

      The array can point to an invalid memory location after using other
      gwyddion functions and lead to application crash. Use with care.

      @param field: the L{gwy.Brick} to view
      @return: array viewing the data
      """
      class gwydfdatap():
         def __init__(self,addr,shape):
            data = (addr,False)
            stride = (8,shape[0]*8,shape[0]*shape[1]*8)
            self.__array_interface__= {
               'strides' : stride,
               'shape'   : shape,
               'data'    : data,
               'typestr' : "|f8",
               'version' : 3}

      addr = brick.get_data_pointer()
      shape = (brick.get_xres(),brick.get_yres(),brick.get_zres())
      return np.array(gwydfdatap(addr,shape),copy=False)

   def data_field_get_data(datafield):
        """Gets the data from a data field.

        The returned array is a copy of the data.
        But it can be safely stored without ever referring to invalid memory.
        """
        return data_field_data_as_array(datafield).copy()

   def data_field_set_data(datafield, data):
        """Sets the data of a data field.

        The data shape must correspond to the data field shape.
        """
        dest = data_field_data_as_array(datafield)
        if dest.shape != data.shape:
           raise ValueError("Data needs same size as the DataField.")
        dest[:] = data

   def brick_get_data(brick):
        """Gets the data from a brick.

        The returned array is a copy of the data.
        But it can be safely stored without ever referring to invalid memory.
        """
        return brick_data_as_array(brick).copy()

   def brick_set_data(brick, data):
        """Sets the data of a brick.

        The data shape must correspond to the brick shape.
        """
        dest = brick_data_as_array(brick)
        if dest.shape != data.shape:
           raise ValueError("Data needs same size as the Brick.")
        dest[:] = data

