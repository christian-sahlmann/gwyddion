Unit GwyddionDump;

interface

type
    (* tDataField structure represent a data field in a way present in the
       dump file and resembling Gwyddion native representation. *)
    pDataField = ^tDataField;
    tDataField = record
        key     : string;     (* name, usually "/0/data" or like *)
        xres    : integer;    (* x sample dimension in pixels *)
        yres    : integer;    (* y sample dimension in pixels *)
        data    : array of double;  (* the data itself, dynamic array, starts
                                       at 0, we use onedimensional array, as
                                       in C, conversion to a twodimensional
                                       one is easy, if required *)
        (* following are optional, may be zero or empty if not present *)
        xreal   : double;     (* physical x dimension in base SI *)
        yreal   : double;     (* physical y dimension in base SI *)
        xyunits : string;     (* units of xreal and yreal *)
        zunits  : string;     (* units of data *)
    end;

    (* Unlike many languages, Pascal has no decent hash table structure in
       standard library.  We use simple (key, value) pairs and inefficient
       but simple sequential search... *)

    (* tMetadata structure represents a single metadata *)
    pMetadata = ^tMetadata;
    tMetadata = record
        key   : string;
        value : string;
    end;

    (* tDump structure represents a complete dump file, with a list of
       named data fields and list of present medatata *)
    pDump = ^tDump;
    tDump = record
        data : array of tDataField;   (* dynamic array, starts at 0 *)
        meta : array of tMetadata;    (* dynamic array, starts at 0 *)
    end;

    (* PluginHelper action specification *)
    tPluginAction = record
        name   : string;              (* action: register, run, load, ... *)
        nargs  : integer;             (* number of required paramstr args *)
        action : function : boolean;  (* function to call to perform it *)
    end;


(*****************************************************************************
    Find metadata or data field, given a key.
    dump: The dump to scan.
    key: The string key.
    Returns: The index in the array, or -1 on failure.
******************************************************************************)
function FindMetadata(dump : tDump;
                      key  : string) : integer;
function FindDataField(dump : tDump;
                       key  : string) : integer;

(*****************************************************************************
    Remove metadata or data field at given position.
    dump: The dump to delete item from.
    index: The position.
******************************************************************************)
procedure RemoveMetadata(var dump : tDump;
                         index    : integer);
procedure RemoveDataField(var dump : tDump;
                          index : integer);

(*****************************************************************************
    Add metadata or data field to an array.
    meta, data: The array to add the item to.
    item: The item to add.
    Returns: The position of the new item in the array.

    NOTE: This is a `safe' function, assuring no two items of the same key
    exist.  If an item with identical key already exists, it is replaced.
******************************************************************************)
function AddMetadata(var dump : tDump;
                     item     : tMetadata) : integer;
function AddDataField(var dump : tDump;
                      item     : tDataField) : integer;

(*****************************************************************************
    Read a Gwyddion plug-in proxy dump file.
    filename : The name of file to read from.
    dump : The dump structure to store it to (existing content is destoryed).
******************************************************************************)
function ReadDump(filename : string;
                  var dump : tDump) : boolean;

(*****************************************************************************
    Write a Gwyddion plug-in proxy dump file.
    filename : The name of file to write to.
    dump : The dump structure to write.
******************************************************************************)
function WriteDump(filename : string;
                   var dump : tDump) : boolean;

function PluginHelper(actions : array of tPluginAction) : boolean;

(********************** IMPLEMENTATION ***********************)
implementation

(*****************************************************************************
    Find metadata or data field, given a key.
    dump: The dump to scan.
    key: The string key.
    Returns: The index in the array, or -1 on failure.
******************************************************************************)
function FindMetadata(dump : tDump;
                      key  : string) : integer;
var
    i, len  : integer;

begin
    FindMetadata := -1;
    len := Length(dump.meta);
    for i := 0 to len-1 do begin
        if dump.meta[i].key = key then begin
            FindMetadata := i;
            Break;
        end;
    end;
end;

function FindDataField(dump : tDump;
                       key  : string) : integer;
var
    i, len  : integer;

begin
    FindDataField := -1;
    len := Length(dump.data);
    for i := 0 to len-1 do begin
        if dump.data[i].key = key then begin
            FindDataField := i;
            Break;
        end;
    end;
end;

(*****************************************************************************
    Remove metadata or data field at given position.
    dump: The dump to delete item from.
    index: The position.
******************************************************************************)
procedure RemoveMetadata(var dump : tDump;
                         index    : integer);
var
    len : integer;

begin
    len := Length(dump.meta);
    if (index >= 0) and (index < len) then begin
        dump.meta[index] := dump.meta[len-1];
        SetLength(dump.meta, len-1);
    end;
end;

procedure RemoveDataField(var dump : tDump;
                          index : integer);
var
    len : integer;

begin
    len := Length(dump.data);
    if (index >= 0) and (index < len) then begin
        dump.data[index] := dump.data[len-1];
        SetLength(dump.data, len-1);
    end;
end;

(*****************************************************************************
    Add metadata or data field to an array.
    meta, data: The array to add the item to.
    item: The item to add.
    Returns: The position of the new item in the array.

    NOTE: This is a `safe' function, assuring no two items of the same key
    exist.  If an item with identical key already exists, it is replaced.
******************************************************************************)
function AddMetadata(var dump : tDump;
                     item     : tMetadata) : integer;
var
    len, i : integer;

begin
    i := FindMetadata(dump, item.key);
    if i < 0 then begin
        len := Length(dump.meta);
        SetLength(dump.meta, len+1);
        dump.meta[len] := item;
        AddMetadata := len;
    end
    else begin
        dump.meta[i] := item;
        AddMetadata := i;
    end;
end;

function AddDataField(var dump : tDump;
                      item     : tDataField) : integer;
var
    len, i : integer;

begin
    i := FindDataField(dump, item.key);
    if i < 0 then begin
        len := Length(dump.data);
        SetLength(dump.data, len+1);
        dump.data[len] := item;
        AddDataField := len;
    end
    else begin
        dump.data[i] := item;
        AddDataField := i;
    end;
end;

(*****************************************************************************
    Read a one line from binary file.
    f : The file to read from.
    Returns: The line (without the trailing NL character), or ''.
******************************************************************************)
function ReadBinLine(var f : file) : string;
var
    linebuf : string;
    b       : char;
    i, len  : integer;

begin
    i := 0;
    len := 0;
    linebuf := '';
    while not EOF(f) do begin
        {$I-}
        BlockRead(f, b, 1);
        {$I+}
        if IOResult <> 0 then begin
            i := 0;
            Break;
        end;

        if Ord(b) = 10 then
            Break;
        i := i+1;
        if len < i then begin
            len := 2*i;
            SetLength(linebuf, len);
        end;
        linebuf[i] := b;
    end;
    SetLength(linebuf, i);
    ReadBinLine := linebuf;
end;

(*****************************************************************************
    Read a one data field from binary file.
    dump: The dump to insert the read data field to.
    f: The file to read from.
    key: The data field key (like '/0/data').
******************************************************************************)
function ReadDataField(var dump : tDump;
                       var f      : file;
                       key      : string) : boolean;
var
    i, size : integer;
    code    : word;
    b, c, d : char;
    dfield  : tDataField;

begin
    dfield.key := key;
    ReadDataField := False;

    (* Get x-resolution information from metadata *)
    i := FindMetadata(dump, key + '/xres');
    if i < 0 then begin
        WriteLn('Uknown xres of DataField ', key);
        Exit;
    end;
    Val(dump.meta[i].value, dfield.xres, code);
    if (code <> 0) or (dfield.xres <= 0) then begin
        WriteLn('Bad xres ', dump.meta[i].value);
        Exit;
    end;
    RemoveMetadata(dump, i);

    (* Get y-resolution information from metadata *)
    i := FindMetadata(dump, key + '/yres');
    if i < 0 then begin
        WriteLn('Uknown yres of DataField ', key);
        Exit;
    end;
    Val(dump.meta[i].value, dfield.yres, code);
    if (code <> 0) or (dfield.yres <= 0) then begin
        WriteLn('Bad yres ', dump.meta[i].value);
        Exit;
    end;
    RemoveMetadata(dump, i);

    (* Read data itself *)
    size := dfield.xres * dfield.yres;
    SetLength(dfield.data, size);
    for i := 0 to size-1 do begin
        {$I-}
        BlockRead(f, dfield.data[i], SizeOf(double));
        {$I+}
        if IOResult <> 0 then begin
            WriteLn('Truncated DataField ', key, ', read only ', i, ' items');
            SetLength(dfield.data, 0);
            Exit;
        end;
    end;

    (* Read and check the ending ']]' and NL *)
    (* FIXME: read it by bytes, I can't read strings from binary
       files directly(!) *)
    {$I-}
    BlockRead(f, b, 1);
    BlockRead(f, c, 1);
    BlockRead(f, d, 1);
    {$I+}
    if (IOResult <> 0) or (b <> ']') or (c <> ']') or (Ord(d) <> 10) then begin
        WriteLn('Missed end of DataField ', key);
        SetLength(dfield.data, 0);
        Exit;
    end;

    (* Get additional data field properties from metadata *)
    dfield.xreal := 0.0;
    i := FindMetadata(dump, key + '/xreal');
    if i >= 0 then begin
        Val(dump.meta[i].value, dfield.xreal, code);
        RemoveMetadata(dump, i);
    end;

    dfield.yreal := 0.0;
    i := FindMetadata(dump, key + '/yreal');
    if i >= 0 then begin
        Val(dump.meta[i].value, dfield.yreal, code);
        RemoveMetadata(dump, i);
    end;

    dfield.xyunits := '';
    i := FindMetadata(dump, key + '/unit-xy');
    if i >= 0 then begin
        dfield.xyunits := dump.meta[i].value;
        RemoveMetadata(dump, i);
    end;

    dfield.zunits := '';
    i := FindMetadata(dump, key + '/unit-z');
    if i >= 0 then begin
        dfield.zunits := dump.meta[i].value;
        RemoveMetadata(dump, i);
    end;

    (* Actually insert (or replace) the data field to the dump *)
    AddDataField(dump, dfield);
    ReadDataField := True;
end;

(*****************************************************************************
    Read a Gwyddion plug-in proxy dump file.
    filename : The name of file to read from.
    dump : The dump structure to store it to (existing content is destoryed).
******************************************************************************)
function ReadDump(filename : string;
                  var dump : tDump) : boolean;
var
    f       : file;         (* the read file *)
    linebuf : string;       (* current line *)
    i       : integer;
    meta    : tMetadata;    (* temporary metadata, before storing it to
                               dump.meta *)
    ok      : boolean;      (* return status *)
    b       : char;

begin
    ok := True;
    (* clear dump to get it to a well-defined state *)
    SetLength(dump.data, 0);
    SetLength(dump.meta, 0);

    AssignFile(f, filename);
    {$I-}
    Reset(f, 1);
    {$I+}
    if IOResult <> 0 then begin
        ReadDump := False;
        Exit;
    end;

    while True do begin
        (* Read a line *)
        linebuf := ReadBinLine(f);
        if linebuf = '' then
            Break;

        (* Parse it to key and value *)
        i := pos('=', linebuf);
        if i = 0 then begin
            WriteLn('Cannot parse line ', linebuf : 32, '...');
            ok := False;
            Break;
        end;
        meta.key := Copy(linebuf, 1, i-1);
        meta.value := Copy(linebuf, i+1, Length(linebuf));

        (* If value is not [, then it's normal metadata, store it.
           We store also data field information here, ReadDataField() pulls
           them from meta later. *)
        if meta.value <> '[' then begin
            AddMetadata(dump, meta);
            Continue;
        end;

        (* The next character must be '[' *)
        {$I-}
        BlockRead(f, b, 1);
        {$I+}
        if b <> '[' then begin
            (* Pascal has no ungetc(), so try to seek one character back,
               but not when we hit EOF *)
            if (IOResult = 0) and not EOF(f) then
                Seek(f, FilePos(f)-1);
            AddMetadata(dump, meta);
            Continue;
        end;

        (* Now it must be a data field, so construct it *)
        if not ReadDataField(dump, f, meta.key) then begin
            ok := False;
            Break;
        end;
    end;
    {$I-}
    Close(f);
    {$I+}

    if not ok then begin
        SetLength(dump.meta, 0);
        SetLength(dump.data, 0);
    end;
    ReadDump := ok;
end;

(*****************************************************************************
    Write a one line to a binary file.
    f : The file to write to.
    s : The line to write, without trailing NL character.
******************************************************************************)
procedure WriteBinLine(var f : file;
                           s : string);
var
    i, len : integer;
    b      : char;

begin
    len := Length(s);
    (* FIXME: write it by bytes, I can't write strings to binary
       files directly(!) *)
    for i := 1 to len do begin
        b := s[i];
        BlockWrite(f, b, 1);
    end;
    b := Chr(10);
    BlockWrite(f, b, 1);
end;

(*****************************************************************************
    Write a Gwyddion plug-in proxy dump file.
    filename : The name of file to write to.
    dump : The dump structure to write.
******************************************************************************)
function WriteDump(filename : string;
                   var dump : tDump) : boolean;
var
    f       : file;         (* the written file *)
    i, len  : integer;
    size, j : integer;
    s       : string;
    b       : char;

begin
    AssignFile(f, filename);
    {$I-}
    Rewrite(f, 1);
    {$I+}
    if IOResult <> 0 then begin
        WriteDump := False;
        Exit;
    end;

    (* Write metadata *)
    len := Length(dump.meta);
    for i := 0 to len-1 do
        WriteBinLine(f, dump.meta[i].key + '=' + dump.meta[i].value);

    (* Write data fields *)
    len := Length(dump.data);
    for i := 0 to len-1 do begin
        with dump.data[i] do begin
            (* X and y resolution must come first *)
            Str(xres, s);
            WriteBinLine(f, key + '/xres=' + s);
            Str(yres, s);
            WriteBinLine(f, key + '/yres=' + s);

            (* If optional data field information is present, write it *)
            if xreal > 0.0 then begin
                Str(xreal, s);
                WriteBinLine(f, key + '/xreal=' + s);
            end;
            if yreal > 0.0 then begin
                Str(yreal, s);
                WriteBinLine(f, key + '/yreal=' + s);
            end;
            if xyunits <> '' then
                WriteBinLine(f, key + '/unit-xy=' + xyunits);
            if zunits <> '' then
                WriteBinLine(f, key + '/unit-z=' + zunits);

            (* Write data field itself *)
            WriteBinLine(f, key + '=[');
            b := '[';
            BlockWrite(f, b, 1);
            size := xres*yres;
            for j := 0 to size-1 do
                BlockWrite(f, data[j], SizeOf(double));
            WriteBinLine(f, ']]');
        end;
    end;

    {$I-}
    Close(f);
    {$I+}
end;

(********************** PLUG-IN HELPER ***********************)
function PluginHelper(actions : array of tPluginAction) : boolean;
var
    i, len  : integer;

begin
    PluginHelper := False;
    len := Length(actions);
    for i := 0 to len-1 do begin
        if (actions[i].nargs = paramcount-1)
            and (actions[i].name = paramstr(1)) then begin
            PluginHelper := actions[i].action();
            Break;
        end;
    end;
end;

end.
(* vim: set ts=4 sw=4 et nocin si errorformat=%f(%l\\\,%c)\ %m : *)
