program invert_pascal;
{$APPTYPE CONSOLE}
uses
  Math, GwyddionDump;

(********************** ACTIONS ***********************)
(* Register: print registration infromation on standard output.
   While it doesn't work realiably under some MS Windows versions, it's good
   to always define anyway *)
function Register : boolean;
begin
    WriteLn('invert_pascal');
    WriteLn('/_Test/Value Invert (Pascal)');
    WriteLn('noninteractive with_defaults');
    Register := True;
end;

(* Run: actually do something with the data.
   The first parameter (paramstr(2)) is run mode,
   the second parameter (paramstr(3)) is the file to read, process, and write
   back. *)
function Run : boolean;
var
    dump     : tDump;
    i, size  : integer;
    min, max : double;
    filename : string;
    run_mode : string;

begin
    run_mode := paramstr(2);
    filename := paramstr(3);
    Run := False;

    (* Possible run modes of this plug-in (as printed by Register) *)
    if (run_mode <> 'noninteractive') and (run_mode <> 'with_defaults') then
        Exit;

    (* Read dump file *)
    if not ReadDump(filename, dump) then
        Exit;

    (* Find tha main data field there *)
    i := FindDataField(dump, '/0/data');
    if i < 0 then
        Exit;

    (* Process the data *)
    with dump.data[i] do begin
        (* Find range so we can properly mirror the data inside it *)
        min := 1.0E300;
        max := -1.0E300;
        size := xres*yres;
        for i := 0 to size-1 do begin
            if data[i] < min then
                min := data[i];
            if data[i] > max then
                max := data[i];
        end;
        (* Invert value inside min..max *)
        for i := 0 to size-1 do
            data[i] := (min + max) - data[i];
    end;

    (* Write back result *)
    Run := WriteDump(filename, dump);
end;

(********************** MAIN ***********************)
var
    actions : array[0..1] of tPluginAction = (
        ( name : 'register'; nargs : 0; action : Register ),
        ( name : 'run';      nargs : 2; action : Run      )
    );

begin
    (* Just call helper with list of possible actions and let it sort them
       out *)
    PluginHelper(actions);
end.

(* vim: set ts=4 sw=4 et nocin si errorformat=%f(%l\\\,%c)\ %m : *)
