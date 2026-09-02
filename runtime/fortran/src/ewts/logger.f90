module logger
  use, intrinsic :: iso_c_binding, only: c_char, c_int, c_double, c_null_char
  use iso_fortran_env, only: output_unit
  use ewts_log_levels, only: ewts_log_level_name
  use ewts_payload_status, only: PAYLOAD_NULL, & 
                                 PAYLOAD_INITTING, &
                                 PAYLOAD_INITTED, &
                                 PAYLOAD_STARTING, &
                                 PAYLOAD_INPROG, &
                                 PAYLOAD_COMPLETE, &
                                 PAYLOAD_ERROR

  implicit none
  private

  integer, parameter, public :: EWTS_NOTSET  = 0
  integer, parameter, public :: EWTS_DEBUG   = 10
  integer, parameter, public :: EWTS_PERFORM = 15
  integer, parameter, public :: EWTS_INFO    = 20
  integer, parameter, public :: EWTS_WARNING = 30
  integer, parameter, public :: EWTS_SEVERE  = 40
  integer, parameter, public :: EWTS_FATAL   = 50
  integer, parameter, public :: EWTS_STATUS  = 60

  character(len=64) :: prefix
  character(len=16) :: val
  integer :: g_mpiRank, status

  type :: logger_state
    character(len=64)   :: ewts_id = "EWTS"
    character(len=8)    :: ewts_id_padded = "EWTS    "
    logical             :: initialized = .false.
    logical             :: enabled = .true.
    integer             :: level_min = EWTS_INFO
    integer             :: unit_log = -1
    character(len=1024) :: path = ""
  end type logger_state

  type(logger_state), allocatable, save :: g_loggers(:)

  public :: write_log, is_logger_enabled, get_log_level, logger_init
  public :: write_log_module, is_logger_enabled_module, get_log_level_module, logger_init_module
  public :: payload_status
  public :: PAYLOAD_NULL
  public :: PAYLOAD_INITTING
  public :: PAYLOAD_INITTED
  public :: PAYLOAD_STARTING
  public :: PAYLOAD_INPROG
  public :: PAYLOAD_COMPLETE
  public :: PAYLOAD_ERROR

#ifdef EWTS_HAVE_NGEN_BRIDGE
  interface
    subroutine ewts_ngen_log(ewts_id, level, message) bind(C, name="ewts_ngen_log")
      import :: c_char, c_int
      character(kind=c_char), dimension(*) :: ewts_id
      integer(c_int), value :: level
      character(kind=c_char), dimension(*) :: message
    end subroutine
  end interface

  interface
    subroutine ewts_ngen_payload_status(ewts_id, status, prog, msg, modnm) bind(C, name="ewts_ngen_payload_status")
        import :: c_char, c_double
        character(kind=c_char), dimension(*) :: ewts_id
        character(kind=c_char), dimension(*) :: status
        real(c_double), value :: prog
        character(kind=c_char), dimension(*) :: msg
        character(kind=c_char), dimension(*) :: modnm
    end subroutine
  end interface
#endif

contains

  subroutine logger_init(id)
    character(len=*), intent(in) :: id
    call logger_init_module(id)
  end subroutine logger_init

  subroutine logger_init_module(id)
    character(len=*), intent(in) :: id
    integer :: idx
    call get_logger_index(id, idx)
  end subroutine logger_init_module

  subroutine upper_inplace(s)
    character(len=*), intent(inout) :: s
    integer :: i, c
    do i = 1, len(s)
      c = iachar(s(i:i))
      if (c >= iachar('a') .and. c <= iachar('z')) s(i:i) = achar(c - 32)
    end do
  end subroutine upper_inplace

  logical function is_ngen_active()
    integer :: lenv
    lenv = 0
    call get_environment_variable("NGEN_RESULTS_DIR", length=lenv)
    is_ngen_active = (lenv > 0)
  end function is_ngen_active

  logical function parse_enabled(v)
    character(len=*), intent(in) :: v
    character(len=32) :: s
    s = adjustl(trim(v))
    call upper_inplace(s)
    if (len_trim(s) == 0) then
      parse_enabled = .true.
    else if (trim(s) == "0" .or. trim(s) == "FALSE" .or. trim(s) == "NO" .or. trim(s) == "OFF" .or. trim(s) == "DISABLED") then
      parse_enabled = .false.
    else
      parse_enabled = .true.
    end if
  end function parse_enabled

  integer function parse_level(v)
    character(len=*), intent(in) :: v
    character(len=32) :: s
    integer :: iostat, num
    s = adjustl(trim(v))
    if (len_trim(s) == 0) then
      parse_level = EWTS_NOTSET
      return
    end if

    read(s, *, iostat=iostat) num
    if (iostat == 0 .and. num >= 0) then
      parse_level = num
      return
    end if

    call upper_inplace(s)
    select case (trim(s))
    case ("DEBUG");   parse_level = EWTS_DEBUG
    case ("PERFORM"); parse_level = EWTS_PERFORM
    case ("INFO");    parse_level = EWTS_INFO
    case ("WARN","WARNING"); parse_level = EWTS_WARNING
    case ("ERROR","SEVERE"); parse_level = EWTS_SEVERE
    case ("FATAL","CRITICAL"); parse_level = EWTS_FATAL
    case ("STATUS"); parse_level = EWTS_STATUS
    case ("NOTSET","NONE"); parse_level = EWTS_NOTSET
    case default; parse_level = EWTS_NOTSET
    end select
  end function parse_level

  character(len=7) function level_name_padded(lvl)
    integer, intent(in) :: lvl
    select case (lvl)
    case (EWTS_DEBUG); level_name_padded = "DEBUG  "
    case (EWTS_PERFORM); level_name_padded = "PERFORM"
    case (EWTS_INFO); level_name_padded = "INFO   "
    case (EWTS_WARNING); level_name_padded = "WARNING"
    case (EWTS_SEVERE); level_name_padded = "SEVERE "
    case (EWTS_FATAL); level_name_padded = "FATAL  "
    case (EWTS_STATUS); level_name_padded = "STATUS "
    case default; level_name_padded = "NOTSET "
    end select
  end function level_name_padded

  subroutine build_ewts_id_padded(id_in, id8)
    character(len=*), intent(in)  :: id_in
    character(len=8), intent(out) :: id8
    character(len=64) :: s
    integer :: n
    s = adjustl(trim(id_in))
    call upper_inplace(s)
    n = len_trim(s)
    if (n >= 8) then
      id8 = s(1:8)
    else if (n > 0) then
      id8 = s(1:n)//repeat(" ", 8-n)
    else
      id8 = "EWTS    "
    end if
  end subroutine build_ewts_id_padded

  subroutine utc_timestamp_iso_ms(ts)
    character(len=*), intent(out) :: ts
    integer :: values(8)
    integer :: zone_min
    integer :: y, mo, d, h, mi, sec, ms
    call date_and_time(values=values)
    zone_min = values(4)
    y=values(1); mo=values(2); d=values(3)
    h=values(5); mi=values(6); sec=values(7); ms=values(8)
    call adjust_utc(y, mo, d, h, mi, zone_min)
    write(ts, "(I4.4,'-',I2.2,'-',I2.2,'T',I2.2,':',I2.2,':',I2.2,'.',I3.3,'Z')") y,mo,d,h,mi,sec,ms
  end subroutine utc_timestamp_iso_ms

  subroutine utc_timestamp_compact(ts)
    character(len=*), intent(out) :: ts
    integer :: values(8)
    integer :: zone_min
    integer :: y, mo, d, h, mi, sec
    call date_and_time(values=values)
    zone_min = values(4)
    y=values(1); mo=values(2); d=values(3)
    h=values(5); mi=values(6); sec=values(7)
    call adjust_utc(y, mo, d, h, mi, zone_min)
    write(ts, "(I4.4,I2.2,I2.2,'T',I2.2,I2.2,I2.2)") y,mo,d,h,mi,sec
  end subroutine utc_timestamp_compact

  subroutine adjust_utc(y, mo, d, h, mi, zone_min)
    integer, intent(inout) :: y, mo, d, h, mi
    integer, intent(in)    :: zone_min
    integer :: total_min
    total_min = h*60 + mi - zone_min
    do while (total_min < 0)
      total_min = total_min + 1440
      call dec_day(y, mo, d)
    end do
    do while (total_min >= 1440)
      total_min = total_min - 1440
      call inc_day(y, mo, d)
    end do
    h = total_min / 60
    mi = mod(total_min, 60)
  end subroutine adjust_utc

  subroutine inc_day(y, mo, d)
    integer, intent(inout) :: y, mo, d
    integer :: dim
    dim = days_in_month(y, mo)
    d = d + 1
    if (d > dim) then
      d = 1
      mo = mo + 1
      if (mo > 12) then
        mo = 1
        y = y + 1
      end if
    end if
  end subroutine inc_day

  subroutine dec_day(y, mo, d)
    integer, intent(inout) :: y, mo, d
    if (d > 1) then
      d = d - 1
    else
      mo = mo - 1
      if (mo < 1) then
        mo = 12
        y = y - 1
      end if
      d = days_in_month(y, mo)
    end if
  end subroutine dec_day

  integer function days_in_month(y, mo)
    integer, intent(in) :: y, mo
    logical :: leap
    leap = (mod(y,4) == 0 .and. (mod(y,100) /= 0 .or. mod(y,400) == 0))
    select case (mo)
    case (1,3,5,7,8,10,12); days_in_month = 31
    case (4,6,9,11); days_in_month = 30
    case (2); days_in_month = merge(29, 28, leap)
    case default; days_in_month = 30
    end select
  end function days_in_month

  function env_key_for(id) result(k)
    character(len=*), intent(in) :: id
    character(len=64) :: k
    character(len=64) :: s
    s = adjustl(trim(id))
    call upper_inplace(s)
    k = trim(s)//"_LOGLEVEL"
  end function env_key_for

  subroutine ensure_registry()
    if (.not. allocated(g_loggers)) then
      allocate(g_loggers(0))
    end if
  end subroutine ensure_registry

  subroutine append_logger(id, idx)
    character(len=*), intent(in) :: id
    integer, intent(out) :: idx
    type(logger_state), allocatable :: tmp(:)
    integer :: n

    call ensure_registry()
    n = size(g_loggers)

    allocate(tmp(n+1))
    if (n > 0) tmp(1:n) = g_loggers
    call move_alloc(tmp, g_loggers)

    idx = n + 1
    g_loggers(idx)%ewts_id = " "
    g_loggers(idx)%ewts_id = adjustl(trim(id))
    call upper_inplace(g_loggers(idx)%ewts_id)
    call build_ewts_id_padded(g_loggers(idx)%ewts_id, g_loggers(idx)%ewts_id_padded)
    g_loggers(idx)%initialized = .false.
    g_loggers(idx)%enabled = .true.
    g_loggers(idx)%level_min = EWTS_INFO
    g_loggers(idx)%unit_log = -1
    g_loggers(idx)%path = ""
  end subroutine append_logger

  subroutine get_logger_index(id, idx)
    character(len=*), intent(in) :: id
    integer, intent(out) :: idx
    character(len=64) :: key
    integer :: i

    key = adjustl(trim(id))
    if (len_trim(key) == 0) key = "EWTS"
    call upper_inplace(key)

    call ensure_registry()
    do i = 1, size(g_loggers)
      if (trim(g_loggers(i)%ewts_id) == trim(key)) then
        idx = i
        call init_logger_state(idx)
        return
      end if
    end do

    call append_logger(trim(key), idx)
    call init_logger_state(idx)
  end subroutine get_logger_index

  subroutine init_logger_state(idx)
    integer, intent(in) :: idx
    integer :: lenv
    character(len=256) :: v
    character(len=64) :: key

    if (g_loggers(idx)%initialized) return
    g_loggers(idx)%initialized = .true.

    call get_environment_variable("EWTS_RANK", val, status=status)

    if (status == 0) then
        read(val, *) g_mpiRank
        write(prefix, '(A,I0,A)') "[rank ", g_mpiRank, "] EWTS"
    else
        prefix = "EWTS"
    end if

    lenv = 0
    call get_environment_variable("EWTS_ENABLED", length=lenv)
    if (lenv > 0) then
      call get_environment_variable("EWTS_ENABLED", v)
      g_loggers(idx)%enabled = parse_enabled(v)
    else
      g_loggers(idx)%enabled = .true.
    end if

    if (g_loggers(idx)%enabled) then
      write(*,'(A)') trim(prefix)  // " " //  trim(g_loggers(idx)%ewts_id) // " logging ENABLED"
    else
      write(*,'(A)') trim(prefix)  // " " //  trim(g_loggers(idx)%ewts_id) // " logging DISABLED"
    end if

    key = env_key_for(g_loggers(idx)%ewts_id)
    lenv = 0
    call get_environment_variable(trim(key), length=lenv)
    if (lenv > 0) then
      call get_environment_variable(trim(key), v)
      g_loggers(idx)%level_min = parse_level(v)
      write(*,'(A)') trim(prefix)  // " " //  trim(g_loggers(idx)%ewts_id) // " log level from env var " // trim(key) // " is " // trim(ewts_log_level_name(g_loggers(idx)%level_min))
    else
      g_loggers(idx)%level_min = EWTS_NOTSET
      write(*,'(A)') trim(prefix)  // " " //  trim(g_loggers(idx)%ewts_id) // " log level env var " // trim(key) // " not found"
    end if

    if (g_loggers(idx)%level_min == EWTS_NOTSET) then
      lenv = 0
      call get_environment_variable("EWTS_LOG_LEVEL", length=lenv)
      if (lenv > 0) then
        call get_environment_variable("EWTS_LOG_LEVEL", v)
        g_loggers(idx)%level_min = parse_level(v)
        write(*,'(A)') trim(prefix)  // " " //  trim(g_loggers(idx)%ewts_id) // " global log level from env var is " // trim(ewts_log_level_name(g_loggers(idx)%level_min))
      else
        g_loggers(idx)%level_min = EWTS_INFO
        write(*,'(A)') trim(prefix)  // " " //  trim(g_loggers(idx)%ewts_id) // " using default log level " // trim(ewts_log_level_name(g_loggers(idx)%level_min))
      end if
      if (g_loggers(idx)%level_min == EWTS_NOTSET) g_loggers(idx)%level_min = EWTS_INFO
    end if

    write(*,'(A)') trim(prefix)  // " " //  trim(g_loggers(idx)%ewts_id) // " log level set to " // trim(ewts_log_level_name(g_loggers(idx)%level_min))

#ifdef EWTS_HAVE_NGEN_BRIDGE
    write(*,'(A)') trim(prefix)  // " " //  trim(g_loggers(idx)%ewts_id) // " using ngen for logging"
#else
    write(*,'(A)') trim(prefix)  // " " //  trim(g_loggers(idx)%ewts_id) // " logging standalone"
#endif

    flush(output_unit)

  end subroutine init_logger_state

  logical function is_logger_enabled()
    integer :: idx
    call get_logger_index("EWTS", idx)
    is_logger_enabled = g_loggers(idx)%enabled
  end function is_logger_enabled

  logical function is_logger_enabled_module(id)
    character(len=*), intent(in) :: id
    integer :: idx
    call get_logger_index(id, idx)
    is_logger_enabled_module = g_loggers(idx)%enabled
  end function is_logger_enabled_module

  integer function get_log_level()
    integer :: idx
    call get_logger_index("EWTS", idx)
    get_log_level = g_loggers(idx)%level_min
  end function get_log_level

  integer function get_log_level_module(id)
    character(len=*), intent(in) :: id
    integer :: idx
    call get_logger_index(id, idx)
    get_log_level_module = g_loggers(idx)%level_min
  end function get_log_level_module

  subroutine open_standalone_file(idx)
    integer, intent(in) :: idx
    integer :: lenv, ios
    character(len=1024) :: dir
    character(len=15) :: ts

    if (g_loggers(idx)%unit_log > 0) return

    lenv = 0
    call get_environment_variable("EWTS_LOG_DIR", length=lenv)
    if (lenv <= 0) then
        g_loggers(idx)%unit_log = -1
        return
    end if

    call get_environment_variable("EWTS_LOG_DIR", dir)
    dir = adjustl(trim(dir))

    call execute_command_line("mkdir -p " // trim(dir), wait=.true.)
    call utc_timestamp_compact(ts)
    g_loggers(idx)%path = trim(dir)//"/"//trim(g_loggers(idx)%ewts_id)//"_"//ts//".log"

    open(newunit=g_loggers(idx)%unit_log, file=trim(g_loggers(idx)%path), status="unknown", &
         position="append", action="write", iostat=ios)
    if (ios /= 0) g_loggers(idx)%unit_log = -1
  end subroutine open_standalone_file

  subroutine call_bridge(ewts_id, lvl, msg)
    integer, intent(in) :: lvl
    character(len=*), intent(in) :: ewts_id
    character(len=*), intent(in) :: msg
    character(kind=c_char), allocatable :: cid(:), cmsg(:)
    integer :: n1, n2, i

    n1 = len_trim(ewts_id)
    n2 = len_trim(msg)

    allocate(cid(n1+1))
    allocate(cmsg(n2+1))

    do i = 1, n1
      cid(i) = transfer(ewts_id(i:i), cid(i))
    end do
    cid(n1+1) = c_null_char

    do i = 1, n2
      cmsg(i) = transfer(msg(i:i), cmsg(i))
    end do
    cmsg(n2+1) = c_null_char

    call ewts_ngen_log(cid, int(lvl, c_int), cmsg)

    deallocate(cid, cmsg)
  end subroutine call_bridge

  subroutine write_log(msg, lvl)
    character(len=*), intent(in) :: msg
    integer, intent(in) :: lvl
    call write_log_module("EWTS", msg, lvl)
  end subroutine write_log

  subroutine write_log_module(id, msg, lvl)
    character(len=*), intent(in) :: id
    character(len=*), intent(in) :: msg
    integer, intent(in) :: lvl
    integer :: idx
    character(len=32) :: ts
    character(len=8) :: id8
    character(len=7) :: lv7

    call get_logger_index(id, idx)
    if (.not. g_loggers(idx)%enabled) return
    if (lvl < g_loggers(idx)%level_min) return

#ifdef EWTS_HAVE_NGEN_BRIDGE
    if (is_ngen_active()) then
      call call_bridge(trim(g_loggers(idx)%ewts_id), lvl, msg)
      return
    end if
#endif

    call open_standalone_file(idx)
    call utc_timestamp_iso_ms(ts)
    id8 = g_loggers(idx)%ewts_id_padded
    lv7 = level_name_padded(lvl)

    if (g_loggers(idx)%unit_log > 0) then
      write(g_loggers(idx)%unit_log, "(A,' ',A,' ',A,' ',A)") trim(ts), id8, lv7, trim(msg)
      flush(g_loggers(idx)%unit_log)
    else
      write(*, "(A,' ',A,' ',A,' ',A)") trim(ts), id8, lv7, trim(msg)
      flush(output_unit)
    end if
  end subroutine write_log_module

  subroutine payload_status(ewts_id, status, prog, msg, modnm)
    use, intrinsic :: iso_c_binding, only: c_char, c_double, c_null_char
    implicit none

    character(len=*), intent(in) :: ewts_id
    character(len=*), intent(in) :: status
    real(c_double), intent(in) :: prog
    character(len=*), intent(in) :: msg
    character(len=*), intent(in) :: modnm

#ifdef EWTS_HAVE_NGEN_BRIDGE
    if (is_ngen_active()) then
      call ewts_ngen_payload_status( &
        trim(ewts_id) // c_null_char, &
        trim(status) // c_null_char, &
        prog, &
        trim(msg) // c_null_char, &
        trim(modnm) // c_null_char)
        return
    end if
#endif
  end subroutine payload_status
  
end module logger
