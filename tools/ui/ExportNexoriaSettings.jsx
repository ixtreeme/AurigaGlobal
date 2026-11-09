// Operates only on a temporary duplicate; original documents and source PSD are preserved.
(function () {
    var previous = app.documents.length ? app.activeDocument : null;
    var oldDialogs = app.displayDialogs, base = null, work = null, opened = false, failed = false;
    var indexed = [], currentSpec, oldRender = $.global.NexoriaRenderExport;
    function indexLayers(group, path) {
        for (var i = 0; i < group.layers.length; i++) {
            var layer = group.layers[i], next = path.concat([layer.name]);
            indexed.push({ layer: layer, path: next, key: next.join('\u001f') });
            if (layer.typename === 'LayerSet') indexLayers(layer, next);
        }
    }
    function progress(message) {
        var file = new File(NX_EXPORT.Output + '/progress.txt');
        file.open('w'); file.write(message); file.close();
    }
    // Cache DOM references and batch visibility changes into one history operation. Repeated
    // path scans and individual history snapshots are extremely costly on the full PSD.
    $.global.NexoriaRenderExport = function () {
        var desired = {}, rows = [], spec = currentSpec;
        for (var i = 0; i < spec.States.length; i++) desired[spec.States[i].Path.join('\u001f')] = spec.States[i].Visible;
        for (var depth = 1; depth <= spec.Path.length; depth++) desired[spec.Path.slice(0, depth).join('\u001f')] = true;
        for (var j = 0; j < indexed.length; j++) {
            var row = indexed[j], visible = desired[row.key] === true;
            for (var parent = 1; visible && parent < row.path.length; parent++)
                if (desired[row.path.slice(0, parent).join('\u001f')] !== true) visible = false;
            rows.push({ layer: row.layer, visible: visible });
        }
        for (var off = rows.length - 1; off >= 0; off--) if (!rows[off].visible && rows[off].layer.visible) rows[off].layer.visible = false;
        for (var on = 0; on < rows.length; on++) if (rows[on].visible && !rows[on].layer.visible) rows[on].layer.visible = true;
    };
    try {
        app.displayDialogs = DialogModes.NO;
        var source = new File(NX_EXPORT.Source);
        for (var i = 0; i < app.documents.length; i++) {
            try { if (app.documents[i].fullName.fsName === source.fsName) base = app.documents[i]; } catch (ignored) {}
        }
        if (!base) { base = app.open(source); opened = true; }
        work = base.duplicate('Nexoria_Unity_Export_' + (new Date()).getTime(), false);
        indexLayers(work, []);
        for (var n = 0; n < NX_EXPORT.Specs.length; n++) {
            currentSpec = NX_EXPORT.Specs[n]; progress((n + 1) + '/' + NX_EXPORT.Specs.length + ' ' + currentSpec.Asset);
            work.suspendHistory('Nexoria isolated export', 'NexoriaRenderExport()');
            app.refresh();
            var options = new PNGSaveOptions(); options.interlaced = false; options.compression = 1;
            work.saveAs(new File(NX_EXPORT.Output + '/' + currentSpec.Asset + '.png'), options, true, Extension.LOWERCASE);
        }
    } catch (error) {
        failed = true;
        if (NX_EXPORT.Error) { var failure = new File(NX_EXPORT.Error); failure.encoding = 'UTF8'; failure.open('w'); failure.write(String(error) + ' line ' + error.line); failure.close(); }
        else throw error;
    } finally {
        if (work) work.close(SaveOptions.DONOTSAVECHANGES);
        if (opened && base) base.close(SaveOptions.DONOTSAVECHANGES);
        app.displayDialogs = oldDialogs;
        if (oldRender === undefined) delete $.global.NexoriaRenderExport; else $.global.NexoriaRenderExport = oldRender;
        if (previous) { try { app.activeDocument = previous; } catch (ignored2) {} }
    }
    if (!failed && NX_EXPORT.Done) { var done = new File(NX_EXPORT.Done); done.open('w'); done.write('Native Photoshop export completed'); done.close(); }
}());
