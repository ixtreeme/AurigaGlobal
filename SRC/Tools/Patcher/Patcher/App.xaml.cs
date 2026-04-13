using System.Windows;

namespace AurigaPatcher
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            if (SelfUpdater.TryHandleSelfUpdate(e.Args))
            {
                Shutdown();
                return;
            }

            base.OnStartup(e);
        }
    }
}
