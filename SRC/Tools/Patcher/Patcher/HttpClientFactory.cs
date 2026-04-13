using System;
using System.Net;
using System.Net.Http;

namespace AurigaPatcher
{
    internal static class HttpClientFactory
    {
        private static readonly Lazy<HttpClient> LazyClient = new Lazy<HttpClient>(CreateClient);

        public static HttpClient Shared
        {
            get { return LazyClient.Value; }
        }

        private static HttpClient CreateClient()
        {
            ServicePointManager.Expect100Continue = false;
            ServicePointManager.UseNagleAlgorithm = false;
            ServicePointManager.DefaultConnectionLimit = Math.Max(ServicePointManager.DefaultConnectionLimit, 16);
            ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12;

            var handler = new HttpClientHandler
            {
                AutomaticDecompression = DecompressionMethods.GZip | DecompressionMethods.Deflate,
                Proxy = null,
                UseProxy = false,
                MaxConnectionsPerServer = 16
               // UseCookies = false
            };

            var client = new HttpClient(handler, true)
            {
                Timeout = TimeSpan.FromMinutes(2)
            };

            client.DefaultRequestHeaders.ConnectionClose = false;
            return client;
        }
    }
}