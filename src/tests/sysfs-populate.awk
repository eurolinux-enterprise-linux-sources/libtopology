BEGIN {
  FS = ":"
}

# match and create a directory (must end with /)
/^[/].+[/]$/ {
  system("mkdir -p " ENVIRON["LIBTOPOLOGY_SYSFS_ROOT"] $1)
}
# match a file with contents and create its parent directory, then
# populate the file. E.g. /devices/system/cpu/cpu1/online:1 should
# create devices/system/cpu/cpu1, then create the "online" file with
# a value of "1"
/^[/].+:.*$/ {
  newfile = $1
  value = $2
  newdir = $1
  # get directory name from full filename
  sub(/\/\w+$/, "/", newdir)
  # print "file = ", newfile, ", value = ", value, ", newdir = ", newdir
  system("mkdir -p " ENVIRON["LIBTOPOLOGY_SYSFS_ROOT"] newdir)
  print value > (ENVIRON["LIBTOPOLOGY_SYSFS_ROOT"] "/" newfile)
}